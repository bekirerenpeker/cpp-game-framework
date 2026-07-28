#include "graphics/ui/TextMeasure.hpp"
#include "utils/Utf8.hpp"
#include "utils/math/MathFuncs.hpp"

namespace Engine {

namespace TextMeasure {

// Both mirror TextRenderer's private constants; if they ever diverge, a measured
// width stops matching the drawn width.
constexpr float TAB_SPACES = 4.0f;
constexpr float FALLBACK_SPACE_ADVANCE = 0.25f;
constexpr uint NO_BREAK = (uint)-1;

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

float lineStep(const Font& font, const TextStyle& style)
{
    return font.getLineHeight(style.size) * style.lineSpacing;
}

float lineBoxHeight(const Font& font, const TextStyle& style)
{
    return font.getAscender(style.size) - font.getDescender(style.size);
}

float heightForLines(const Font& font, const TextStyle& style, uint lineCount)
{
    if (lineCount == 0) return 0.0f;
    return (float)(lineCount - 1) * lineStep(font, style) + lineBoxHeight(font, style);
}

float measureRun(const Font& font, std::string_view text, const TextStyle& style)
{
    if (!font.isValid()) return 0.0f;

    float width = 0.0f;
    uint32_t prev = 0;
    size_t i = 0;
    while (i < text.size()) {
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

float measureLongestWord(const Font& font, std::string_view text, const TextStyle& style)
{
    float longest = 0.0f;
    size_t start = 0;
    for (size_t i = 0; i <= text.size(); i++) {
        bool atEnd = i == text.size();
        char c = atEnd ? ' ' : text[i];
        if (!atEnd && c != ' ' && c != '\t' && c != '\n' && c != '\r') continue;

        if (i > start) {
            float width = measureRun(font, text.substr(start, i - start), style);
            longest = Math::max(longest, width);
        }
        start = i + 1;
    }
    return longest;
}

float measureLongestLine(const Font& font, std::string_view text, const TextStyle& style)
{
    float longest = 0.0f;
    size_t start = 0;
    for (size_t i = 0; i <= text.size(); i++) {
        if (i != text.size() && text[i] != '\n') continue;

        float width = measureRun(font, text.substr(start, i - start), style);
        longest = Math::max(longest, width);
        start = i + 1;
    }
    return longest;
}

uint wrap(
    const Font& font, std::string_view text, const TextStyle& style, float maxWidth,
    std::vector<LayoutLine>& lines
)
{
    if (!font.isValid()) return 0;

    float step = lineStep(font, style);
    // A zero or negative budget means "no constraint" rather than "break every glyph",
    // which would otherwise emit one line per character for an empty box.
    bool bounded = maxWidth > 0.0001f;

    uint produced = 0;
    uint lineStartByte = 0;
    float lineWidth = 0.0f;
    uint breakByte = NO_BREAK;
    float widthAtBreak = 0.0f;
    uint32_t prev = 0;

    size_t i = 0;
    while (i < text.size()) {
        size_t charStart = i;
        uint32_t codepoint = Utf8::next(text, i);
        if (codepoint == 0) break;
        if (codepoint == '\r') continue;

        if (codepoint == '\n') {
            lines.push_back({lineStartByte, (uint)charStart - lineStartByte, lineWidth, step});
            produced++;
            lineStartByte = (uint)i;
            lineWidth = 0.0f;
            breakByte = NO_BREAK;
            prev = 0;
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
            breakByte = (uint)i;
            widthAtBreak = lineWidth;
        } else if (bounded && lineWidth > 0.0f && lineWidth + advance > maxWidth) {
            if (breakByte != NO_BREAK && breakByte > lineStartByte) {
                lines.push_back({lineStartByte, breakByte - lineStartByte, widthAtBreak, step});
                produced++;
                i = breakByte;
            } else {
                // A single word wider than the whole line still has to go somewhere,
                // so it breaks mid-word rather than overflowing without limit.
                lines.push_back({lineStartByte, (uint)charStart - lineStartByte, lineWidth, step});
                produced++;
                i = charStart;
            }
            lineStartByte = (uint)i;
            lineWidth = 0.0f;
            breakByte = NO_BREAK;
            prev = 0;
            continue;
        }

        lineWidth += advance;
        prev = codepoint;
    }

    lines.push_back({lineStartByte, (uint)text.size() - lineStartByte, lineWidth, step});
    produced++;
    return produced;
}

}   // namespace TextMeasure

}   // namespace Engine
