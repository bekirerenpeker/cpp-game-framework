#include "graphics/ui/elements/TextMeasure.hpp"
#include "utils/Utf8.hpp"
#include "utils/math/MathFuncs.hpp"

namespace Engine {

namespace TextMeasure {

// Both mirror TextRenderer's private constants; if they ever diverge, a measured
// width stops matching the drawn width.
constexpr float TAB_SPACES = 4.0f;
constexpr float FALLBACK_SPACE_ADVANCE = 0.25f;

static const Glyph* resolveGlyph(const Font& font, uint32_t codepoint)
{
    const Glyph* glyph = font.getGlyph(codepoint);
    if (glyph) return glyph;
    return font.getGlyph('?');
}

static float spaceAdvance(const Font& font)
{
    const Glyph* space = font.getGlyph(' ');
    return space ? space->advance : FALLBACK_SPACE_ADVANCE;
}

static bool isBreakSpace(uint32_t codepoint) { return codepoint == ' ' || codepoint == '\t'; }

const TextStyle& pickStyle(
    const TextSpan& span, const TextStyle& defaultStyle, const std::vector<TextStyle>& spanStyles
)
{
    if (span.styleIndex < 0) return defaultStyle;
    size_t index = (size_t)span.styleIndex;
    return index < spanStyles.size() ? spanStyles[index] : defaultStyle;
}

float lineStep(const Font& font, const TextStyle& style)
{
    return font.getLineHeight(style.size) * style.lineSpacing;
}

float lineBoxHeight(const Font& font, const TextStyle& style)
{
    return font.getAscender(style.size) - font.getDescender(style.size);
}

float measureRun(const Font& font, std::string_view text, const TextStyle& style)
{
    if (!font.isValid()) return 0.0f;

    float width = 0.0f;
    uint32_t prev = 0;
    size_t i = 0;
    while (i < text.size()) {
        if (TextTags::isEscapedTagAt(text, i)) {
            i++;
            continue;
        }

        uint32_t codepoint = Utf8::next(text, i);
        if (codepoint == 0) break;
        if (codepoint == '\r' || codepoint == '\n') {
            prev = 0;
            continue;
        }

        if (codepoint == '\t') {
            width += (spaceAdvance(font) * TAB_SPACES + style.letterSpacing) * style.size;
            prev = 0;
            continue;
        }

        const Glyph* glyph = resolveGlyph(font, codepoint);
        if (!glyph) {
            prev = 0;
            continue;
        }

        if (prev != 0) width += font.getKerning(prev, codepoint) * style.size;
        width += (glyph->advance + style.letterSpacing) * style.size;
        prev = codepoint;
    }
    return width;
}

LeafWidths measureSpanWidths(
    const Font& font, const std::vector<TextSpan>& spans, const TextStyle& defaultStyle,
    const std::vector<TextStyle>& spanStyles, bool wrapEnabled
)
{
    LeafWidths widths;
    if (!font.isValid()) return widths;

    float lineWidth = 0.0f;
    float wordWidth = 0.0f;

    for (const TextSpan& span : spans) {
        const TextStyle& style = pickStyle(span, defaultStyle, spanStyles);
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

            float advance = 0.0f;
            if (codepoint == '\t') {
                advance = (spaceAdvance(font) * TAB_SPACES + style.letterSpacing) * style.size;
                prev = 0;
            } else {
                const Glyph* glyph = resolveGlyph(font, codepoint);
                if (!glyph) {
                    prev = 0;
                    continue;
                }
                advance = (glyph->advance + style.letterSpacing) * style.size;
                if (prev != 0) advance += font.getKerning(prev, codepoint) * style.size;
                prev = codepoint;
            }

            lineWidth += advance;
            // A word is allowed to straddle a span boundary, so the accumulator is
            // deliberately not reset between spans -- only at whitespace.
            if (isBreakSpace(codepoint)) {
                widths.min = Math::max(widths.min, wordWidth);
                wordWidth = 0.0f;
            } else {
                wordWidth += advance;
            }
        }
    }

    widths.max = Math::max(widths.max, lineWidth);
    widths.min = Math::max(widths.min, wordWidth);
    if (!wrapEnabled) widths.min = widths.max;
    return widths;
}

float wrapSpans(
    const Font& font, const std::vector<TextSpan>& spans, const TextStyle& defaultStyle,
    const std::vector<TextStyle>& spanStyles, float maxWidth, bool fixedLineHeight,
    std::vector<TextRun>& runs, std::vector<LayoutLine>& lines
)
{
    runs.clear();
    lines.clear();
    if (!font.isValid()) return 0.0f;

    const bool bounded = maxWidth > 0.0001f;
    const float defaultAscent = font.getAscender(defaultStyle.size);
    const float defaultDescent = -font.getDescender(defaultStyle.size);
    const float defaultStep = lineStep(font, defaultStyle);

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
        float ascent = fixedLineHeight || lineAscent <= 0.0f ? defaultAscent : lineAscent;
        float descent = fixedLineHeight || lineDescent <= 0.0f ? defaultDescent : lineDescent;
        float height = fixedLineHeight || lineHeight <= 0.0f ? defaultStep : lineHeight;

        // The baseline can only be placed once the whole line is known, so every run
        // on it gets its y patched here rather than when it was emitted.
        float baseline = lineTop + ascent;
        for (size_t i = lineRunStart; i < runs.size(); i++) runs[i].offset.y = baseline;

        LayoutLine line;
        line.width = width;
        line.height = height;
        line.ascent = ascent;
        line.descent = descent;
        lines.push_back(line);

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
        const TextStyle& style = pickStyle(spans[spanIdx], defaultStyle, spanStyles);

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
        lineHeight = Math::max(lineHeight, lineStep(font, style));

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

        float advance = 0.0f;
        if (codepoint == '\t') {
            advance = (spaceAdvance(font) * TAB_SPACES + style.letterSpacing) * style.size;
        } else {
            const Glyph* glyph = resolveGlyph(font, codepoint);
            if (!glyph) {
                prev = 0;
                continue;
            }
            advance = (glyph->advance + style.letterSpacing) * style.size;
            if (prev != 0) advance += font.getKerning(prev, codepoint) * style.size;
        }

        if (isBreakSpace(codepoint)) {
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
        } else if (bounded && penX > 0.0f && penX + advance > maxWidth) {
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

        penX += advance;
        prev = codepoint;
    }

    closeLine(penX);

    float height = 0.0f;
    for (size_t i = 0; i + 1 < lines.size(); i++) height += lines[i].height;
    if (!lines.empty()) height += lines.back().ascent + lines.back().descent;
    return height;
}

}   // namespace TextMeasure

}   // namespace Engine
