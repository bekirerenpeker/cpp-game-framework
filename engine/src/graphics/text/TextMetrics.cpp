#include "graphics/text/TextMetrics.hpp"
#include "graphics/text/TextTags.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "utils/Utf8.hpp"

namespace Engine {

namespace TextMetrics {

namespace {

// Every walk in the engine funnels through resolveGlyph, so warning per miss would
// bury the log under one line per glyph per frame.
bool g_warnedMissingGlyph = false;

}   // namespace

const Glyph* resolveGlyph(const Font& font, uint32_t codepoint)
{
    const Glyph* glyph = font.getGlyph(codepoint);
    if (glyph) return glyph;

    if (!g_warnedMissingGlyph) {
        g_warnedMissingGlyph = true;
        LOG_WARNING(
            "font {} has no glyph for codepoint {}; substituting '?' where it exists "
            "(further misses stay silent)",
            font.getSourcePath(), codepoint
        );
    }
    return font.getGlyph('?');
}

float spaceAdvance(const Font& font)
{
    const Glyph* space = font.getGlyph(' ');
    return space ? space->advance : FALLBACK_SPACE_ADVANCE;
}

float lineStep(const Font& font, const TextStyle& style)
{
    return font.getLineHeight(style.size) * style.lineSpacing;
}

float lineBoxHeight(const Font& font, const TextStyle& style)
{
    return font.getAscender(style.size) - font.getDescender(style.size);
}

bool isBreakSpace(uint32_t codepoint) { return codepoint == ' ' || codepoint == '\t'; }

// The single rule for turning a codepoint into an advance. Both the layout
// calculator (measuring) and TextRenderer::drawSpan (emitting) go through this, so
// a measured width cannot drift from a drawn one. '\r' and '\n' are control flow
// and are handled by the callers, never passed in here.
GlyphStep step(const Font& font, const TextStyle& style, uint32_t codepoint, uint32_t prev)
{
    GlyphStep result;

    // A tab is a fixed jump with no glyph, and it breaks the kerning pair: there is
    // no pair to form against whitespace, and the old walks disagreed about this.
    if (codepoint == '\t') {
        result.advance = (spaceAdvance(font) * TAB_SPACES + style.letterSpacing) * style.size;
        return result;
    }

    result.glyph = resolveGlyph(font, codepoint);
    if (!result.glyph) return result;

    result.advance = (result.glyph->advance + style.letterSpacing) * style.size;
    if (prev != 0) result.kerning = font.getKerning(prev, codepoint) * style.size;
    result.kerningPrev = codepoint;
    return result;
}

float measure(const Font& font, const TextStyle& style, std::string_view text)
{
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

        GlyphStep glyphStep = step(font, style, codepoint, prev);
        prev = glyphStep.kerningPrev;
        width += glyphStep.total();
    }
    return width;
}

}   // namespace TextMetrics

}   // namespace Engine
