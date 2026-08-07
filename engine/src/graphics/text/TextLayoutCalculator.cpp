#include "graphics/text/TextLayoutCalculator.hpp"
#include "graphics/text/FontLoader.hpp"
#include "graphics/text/TextMetrics.hpp"
#include "utils/Utf8.hpp"
#include "utils/math/MathFuncs.hpp"

namespace Engine {

namespace {

constexpr float BOUNDED_EPSILON = 0.0001f;

// Three ASCII dots rather than U+2026: the marker has to exist in whatever face is
// baked, and a single-glyph ellipsis sits outside Latin-1.
constexpr std::string_view ELLIPSIS = "...";

// The last break-space seen on the current line, saved in case a later word doesn't
// fit and the walk has to backtrack to it.
struct WrapPoint
{
    size_t span = 0, byte = 0;
    size_t runCount = 0, runByte = 0;
    float runX = 0.0f, width = 0.0f;
    float ascent = 0.0f, descent = 0.0f, height = 0.0f;
};

}   // namespace

// Which font this block actually lays out with, and whether that just changed: the
// pointer catches the swap from the borrowed default to the real face, the version
// catches that same font's atlas arriving under it. A compare beats every font pushing
// a dirty flag to every block that ever used it.
void TextLayoutCalculator::syncFontVersion(TextBlock& block)
{
    const Font* resolved = FontLoader::get().resolve(block.m_font);
    uint version = resolved ? resolved->getLoadVersion() : 0;

    if (resolved == block.m_resolvedFont && version == block.m_fontVersion) return;

    block.m_resolvedFont = resolved;
    block.m_fontVersion = version;
    block.invalidate();
}

TextBlockWidths TextLayoutCalculator::measureMinMaxWidth(TextBlock& block)
{
    syncFontVersion(block);
    if (!block.m_widthsDirty) return block.m_widths;
    block.m_widthsDirty = false;

    block.m_widths = TextBlockWidths {};
    if (!block.m_resolvedFont || !block.m_resolvedFont->isValid()) return block.m_widths;

    block.ensureSpans();
    const Font& font = *block.m_resolvedFont;

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
    syncFontVersion(block);
    if (!block.isDirtyFor(availableWidth)) return block.m_bounds.y;

    block.clearLayout();
    block.m_lastWidth = availableWidth;
    block.m_layoutDirty = false;

    if (!block.m_resolvedFont || !block.m_resolvedFont->isValid()) return 0.0f;

    block.ensureSpans();

    const Font& font = *block.m_resolvedFont;
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
    WrapPoint lastBreak;

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
            lastBreak = {spanIdx, byteIdx,    runs.size(), runByte,   runX,
                         penX,    lineAscent, lineDescent, lineHeight};
        } else if (bounded && penX > 0.0f && penX + glyphStep.total() > budget) {
            if (haveBreak) {
                // The last space can be several spans back, so everything emitted
                // since is discarded and the walk resumes from there.
                runs.resize(lastBreak.runCount);
                runSpan = lastBreak.span;
                runByte = lastBreak.runByte;
                runX = lastBreak.runX;
                emitRun(lastBreak.byte);

                lineAscent = lastBreak.ascent;
                lineDescent = lastBreak.descent;
                lineHeight = lastBreak.height;
                closeLine(lastBreak.width);

                spanIdx = lastBreak.span;
                byteIdx = lastBreak.byte;
                startRun(lastBreak.span, lastBreak.byte);
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
    applyEllipsis(block, availableWidth);

    // Aligned inside the width it was given rather than only inside its own longest
    // line, or right-aligned text that never wraps would have no slack to move in. The
    // max keeps a single unbreakable word from producing negative slack, and a caller
    // that passed no width (world-space text) still aligns against its own bounds.
    applyHorizontalAlign(block, Math::max(availableWidth, block.m_bounds.x));
    return height;
}

// Run before alignment, since cutting a line back changes the slack it is aligned
// against. Lines that already fit are copied through untouched, and a block whose
// widest line fits returns before any of this, so the common case costs one compare.
void TextLayoutCalculator::applyEllipsis(TextBlock& block, float availableWidth)
{
    if (block.m_overflow != TextOverflow::Ellipsis) return;
    if (availableWidth <= BOUNDED_EPSILON) return;
    if (block.m_bounds.x <= availableWidth + BOUNDED_EPSILON) return;

    const Font& font = *block.m_resolvedFont;

    // Rebuilt rather than edited in place: dropping runs from the middle would shift
    // every later line's firstRun, and one forward pass is cheaper than patching them.
    std::vector<TextRun> kept;
    kept.reserve(block.m_runs.size() + block.m_lines.size());

    float widest = 0.0f;
    for (TextLine& line : block.m_lines) {
        uint firstRun = (uint)kept.size();

        if (line.runCount == 0 || line.width <= availableWidth + BOUNDED_EPSILON) {
            for (uint i = 0; i < line.runCount; i++)
                kept.push_back(block.m_runs[line.firstRun + i]);

            line.firstRun = firstRun;
            widest = Math::max(widest, line.width);
            continue;
        }

        // The marker takes the style of the run the cut lands in, so it reads as part of
        // the text it stands in for rather than as the block's default leaking through.
        const TextRun& lastRun = block.m_runs[line.firstRun + line.runCount - 1];
        const TextStyle& markerStyle = block.styleForRun(lastRun.styleIndex);
        float markerWidth = TextMetrics::measure(font, markerStyle, ELLIPSIS);

        float lineStart = block.m_runs[line.firstRun].offset.x;
        float budget = lineStart + availableWidth - markerWidth;

        float pen = lineStart;
        bool isCut = false;
        for (uint i = 0; i < line.runCount && !isCut; i++) {
            const TextRun& run = block.m_runs[line.firstRun + i];
            const TextStyle& style = block.styleForRun(run.styleIndex);

            uint32_t prev = 0;
            size_t byteIdx = 0;
            while (byteIdx < run.text.size()) {
                if (TextTags::isEscapedTagAt(run.text, byteIdx)) {
                    byteIdx++;
                    continue;
                }

                size_t charStart = byteIdx;
                uint32_t codepoint = Utf8::next(run.text, byteIdx);
                if (codepoint == 0) break;

                GlyphStep glyphStep = TextMetrics::step(font, style, codepoint, prev);
                if (pen + glyphStep.total() > budget) {
                    byteIdx = charStart;
                    isCut = true;
                    break;
                }

                prev = glyphStep.kerningPrev;
                pen += glyphStep.total();
            }

            if (byteIdx == 0) continue;

            TextRun head = run;
            head.text = run.text.substr(0, byteIdx);
            kept.push_back(head);
        }

        TextRun marker;
        marker.text = ELLIPSIS;
        marker.styleIndex = lastRun.styleIndex;
        marker.offset = Vec2(pen, block.m_runs[line.firstRun].offset.y);
        kept.push_back(marker);

        line.firstRun = firstRun;
        line.width = pen + markerWidth - lineStart;
        widest = Math::max(widest, line.width);
    }

    // Counts are settled after the fact, since a line only knows how many runs it kept
    // once the next line's start is known.
    for (size_t i = 0; i < block.m_lines.size(); i++) {
        uint end = i + 1 < block.m_lines.size() ? block.m_lines[i + 1].firstRun : (uint)kept.size();
        block.m_lines[i].runCount = end - block.m_lines[i].firstRun;
    }

    block.m_runs = std::move(kept);
    block.m_bounds.x = widest;
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
