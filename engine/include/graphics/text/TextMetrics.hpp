#pragma once

#include "graphics/text/Font.hpp"
#include "graphics/text/TextStyle.hpp"
#include <cstdint>
#include <string_view>

namespace Engine {

// The result of consuming one codepoint. glyph is null when there is nothing to
// draw (a tab, or a codepoint with no substitute) but the widths are still valid;
// kerningPrev is what the caller carries into the next step, 0 breaking the pair.
// kerning and advance are kept apart because they land either side of the glyph:
// kerning moves the pen before it is drawn, advance after. Measuring only ever
// needs total(), which is why folding them together looks harmless and is not.
struct GlyphStep
{
    const Glyph* glyph = nullptr;
    float kerning = 0.0f;
    float advance = 0.0f;
    uint32_t kerningPrev = 0;

    float total() const { return kerning + advance; }
};

namespace TextMetrics {

constexpr float TAB_SPACES = 4.0f;
constexpr float FALLBACK_SPACE_ADVANCE = 0.25f;

GlyphStep step(const Font& font, const TextStyle& style, uint32_t codepoint, uint32_t prev);
// The same walk the layout and the measure both run, so a width worked out here cannot
// drift from where the glyphs actually land -- which is what a caret depends on.
float measure(const Font& font, const TextStyle& style, std::string_view text);

const Glyph* resolveGlyph(const Font& font, uint32_t codepoint);
float spaceAdvance(const Font& font);
float lineStep(const Font& font, const TextStyle& style);
float lineBoxHeight(const Font& font, const TextStyle& style);
bool isBreakSpace(uint32_t codepoint);

}   // namespace TextMetrics

}   // namespace Engine
