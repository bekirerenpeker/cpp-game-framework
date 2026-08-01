#include "graphics/text/TextLayoutCalculator.hpp"
#include "graphics/text/TextMetrics.hpp"
#include "utils/Utf8.hpp"
#include "utils/math/MathFuncs.hpp"

namespace Engine {

namespace {

constexpr float BOUNDED_EPSILON = 0.0001f;

}   // namespace

TextBlockWidths TextLayoutCalculator::measureMinMaxWidth(TextBlock& block)
{
    if (!block.m_widthsDirty) return block.m_widths;
    block.m_widthsDirty = false;

    block.m_widths = TextBlockWidths {};
    if (!block.m_font || !block.m_font->isValid()) return block.m_widths;

    block.ensureSpans();
    const Font& font = *block.m_font;

    TextBlockWidths widths;
    float lineWidth = 0.0f;
    float wordWidth = 0.0f;

    for (const TextSpan& span : block.m_spans) {
        const TextStyle& style = block.styleForRun(span.styleIndex);
        std::string_view text = span.text;
        uint32_t prev = 0;

        size_t i = 0;
        while (i < text.size()) {
            if (TextTags::isEscapedTagAt(text, i)) {
                i++;
                continue;
            }

            uint32_t codepoint = Utf8::next(text, i);
            if (codepoint == 0) break;
            if (codepoint == '\r') continue;

            if (codepoint == '\n') {
                widths.max = Math::max(widths.max, lineWidth);
                widths.min = Math::max(widths.min, wordWidth);
                lineWidth = 0.0f;
                wordWidth = 0.0f;
                prev = 0;
                continue;
            }

            GlyphStep glyphStep = TextMetrics::step(font, style, codepoint, prev);
            prev = glyphStep.kerningPrev;

            lineWidth += glyphStep.total();
            // A word is allowed to straddle a span boundary, so the accumulator is
            // deliberately not reset between spans -- only at whitespace.
            if (TextMetrics::isBreakSpace(codepoint)) {
                widths.min = Math::max(widths.min, wordWidth);
                wordWidth = 0.0f;
            } else {
                wordWidth += glyphStep.total();
            }
        }
    }

    widths.max = Math::max(widths.max, lineWidth);
    widths.min = Math::max(widths.min, wordWidth);
    if (!block.m_wrapEnabled) widths.min = widths.max;

    block.m_widths = widths;
    return widths;
}

float TextLayoutCalculator::calculate(TextBlock& block, float availableWidth)
{
    if (!block.isDirtyFor(availableWidth)) return block.m_bounds.y;

    block.clearLayout();
    block.m_lastWidth = availableWidth;
    block.m_layoutDirty = false;

    if (!block.m_font || !block.m_font->isValid()) return 0.0f;

    block.ensureSpans();

    const Font& font = *block.m_font;
    const std::vector<TextSpan>& spans = block.m_spans;
    std::vector<TextRun>& runs = block.m_runs;
    std::vector<TextLine>& lines = block.m_lines;

    // A zero budget breaks only on explicit newlines, which is exactly the
    // non-wrapping behaviour -- no second code path needed.
    const float budget = block.m_wrapEnabled ? availableWidth : 0.0f;
    const bool bounded = budget > BOUNDED_EPSILON;

    const float defaultAscent = font.getAscender(block.m_style.size);
    const float defaultDescent = -font.getDescender(block.m_style.size);
    const float defaultStep = TextMetrics::lineStep(font, block.m_style);

    size_t spanIdx = 0;
    size_t byteIdx = 0;
    size_t runSpan = 0;
    size_t runByte = 0;
    float runX = 0.0f;
    float penX = 0.0f;
    float lineTop = 0.0f;
    size_t lineRunStart = 0;
    float lineAscent = 0.0f;
    float lineDescent = 0.0f;
    float lineHeight = 0.0f;
    float widest = 0.0f;
    uint32_t prev = 0;

    bool haveBreak = false;
    size_t breakSpan = 0, breakByte = 0, breakRunCount = 0, breakRunByte = 0;
    float breakRunX = 0.0f, breakWidth = 0.0f;
    float breakAscent = 0.0f, breakDescent = 0.0f, breakHeight = 0.0f;

    auto emitRun = [&](size_t endByte) {
        if (runSpan >= spans.size() || endByte <= runByte) return;
        TextRun run;
        run.text = spans[runSpan].text.substr(runByte, endByte - runByte);
        run.styleIndex = spans[runSpan].styleIndex;
        run.offset = Vec2(runX, 0.0f);
        runs.push_back(run);
    };

    auto startRun = [&](size_t span, size_t byte) {
        runSpan = span;
        runByte = byte;
        runX = penX;
    };

    auto closeLine = [&](float width) {
        float ascent = block.m_fixedLineHeight || lineAscent <= 0.0f ? defaultAscent : lineAscent;
        float descent =
            block.m_fixedLineHeight || lineDescent <= 0.0f ? defaultDescent : lineDescent;
        float height = block.m_fixedLineHeight || lineHeight <= 0.0f ? defaultStep : lineHeight;

        // The baseline can only be placed once the whole line is known, so every run
        // on it gets its y patched here rather than when it was emitted.
        float baseline = lineTop + ascent;
        for (size_t i = lineRunStart; i < runs.size(); i++) runs[i].offset.y = baseline;

        TextLine line;
        line.top = lineTop;
        line.width = width;
        line.height = height;
        line.ascent = ascent;
        line.descent = descent;
        line.firstRun = (uint)lineRunStart;
        line.runCount = (uint)(runs.size() - lineRunStart);
        lines.push_back(line);

        widest = Math::max(widest, width);
        lineTop += height;
        lineRunStart = runs.size();
        lineAscent = 0.0f;
        lineDescent = 0.0f;
        lineHeight = 0.0f;
        penX = 0.0f;
        haveBreak = false;
        prev = 0;
    };

    if (!spans.empty()) startRun(0, 0);

    while (spanIdx < spans.size()) {
        std::string_view text = spans[spanIdx].text;
        const TextStyle& style = block.styleForRun(spans[spanIdx].styleIndex);

        if (byteIdx >= text.size()) {
            emitRun(text.size());
            spanIdx++;
            byteIdx = 0;
            prev = 0;
            if (spanIdx < spans.size()) startRun(spanIdx, 0);
            continue;
        }

        // A line is as tall as the tallest style touching it, so every span that
        // enters the line raises its metrics and the baseline is only fixed at close.
        lineAscent = Math::max(lineAscent, font.getAscender(style.size));
        lineDescent = Math::max(lineDescent, -font.getDescender(style.size));
        lineHeight = Math::max(lineHeight, TextMetrics::lineStep(font, style));

        if (TextTags::isEscapedTagAt(text, byteIdx)) {
            byteIdx++;
            continue;
        }

        size_t charStart = byteIdx;
        uint32_t codepoint = Utf8::next(text, byteIdx);
        if (codepoint == 0) {
            byteIdx = text.size();
            continue;
        }
        if (codepoint == '\r') continue;

        if (codepoint == '\n') {
            emitRun(charStart);
            closeLine(penX);
            startRun(spanIdx, byteIdx);
            continue;
        }

        GlyphStep glyphStep = TextMetrics::step(font, style, codepoint, prev);

        if (TextMetrics::isBreakSpace(codepoint)) {
            haveBreak = true;
            breakSpan = spanIdx;
            breakByte = byteIdx;
            breakWidth = penX;
            breakRunCount = runs.size();
            breakRunByte = runByte;
            breakRunX = runX;
            breakAscent = lineAscent;
            breakDescent = lineDescent;
            breakHeight = lineHeight;
        } else if (bounded && penX > 0.0f && penX + glyphStep.total() > budget) {
            if (haveBreak) {
                // The last space can be several spans back, so everything emitted
                // since is discarded and the walk resumes from there.
                runs.resize(breakRunCount);
                runSpan = breakSpan;
                runByte = breakRunByte;
                runX = breakRunX;
                emitRun(breakByte);

                lineAscent = breakAscent;
                lineDescent = breakDescent;
                lineHeight = breakHeight;
                closeLine(breakWidth);

                spanIdx = breakSpan;
                byteIdx = breakByte;
                startRun(breakSpan, breakByte);
            } else {
                // A single word wider than the whole line still has to go somewhere.
                emitRun(charStart);
                closeLine(penX);
                byteIdx = charStart;
                startRun(spanIdx, charStart);
            }
            continue;
        }

        penX += glyphStep.total();
        prev = glyphStep.kerningPrev;
    }

    closeLine(penX);

    // Trailing line spacing is trimmed: the block ends at the last line's descender
    // rather than at where the next line would have started.
    float height = 0.0f;
    for (size_t i = 0; i + 1 < lines.size(); i++) height += lines[i].height;
    if (!lines.empty()) height += lines.back().ascent + lines.back().descent;

    block.m_bounds = Vec2(widest, height);
    applyHorizontalAlign(block, bounded ? budget : widest);
    return height;
}

// Baked into the run offsets rather than applied at draw, because the width to
// align against is already final here. A shrink-to-fit block aligns against its own
// widest line, which is what makes centring a no-op when nothing is stretching it.
void TextLayoutCalculator::applyHorizontalAlign(TextBlock& block, float contentWidth)
{
    if (block.m_alignH == TextAlignH::Left) return;

    for (const TextLine& line : block.m_lines) {
        float slack = contentWidth - line.width;
        if (slack == 0.0f) continue;

        float delta = block.m_alignH == TextAlignH::Center ? slack * 0.5f : slack;
        for (uint i = 0; i < line.runCount; i++) {
            block.m_runs[line.firstRun + i].offset.x += delta;
        }
    }
}

}   // namespace Engine
