#pragma once

#include "graphics/text/Font.hpp"
#include "graphics/text/TextStyle.hpp"
#include <cstdint>

namespace Engine {

// The result of consuming one codepoint. glyph is null when there is nothing to
// draw (a tab, or a codepoint with no substitute) but advance is still valid;
// kerningPrev is what the caller carries into the next step, 0 breaking the pair.
struct GlyphStep
{
    const Glyph* glyph = nullptr;
    float advance = 0.0f;
    uint32_t kerningPrev = 0;
};

namespace TextMetrics {

constexpr float TAB_SPACES = 4.0f;
constexpr float FALLBACK_SPACE_ADVANCE = 0.25f;

GlyphStep step(const Font& font, const TextStyle& style, uint32_t codepoint, uint32_t prev);

const Glyph* resolveGlyph(const Font& font, uint32_t codepoint);
float spaceAdvance(const Font& font);
float lineStep(const Font& font, const TextStyle& style);
float lineBoxHeight(const Font& font, const TextStyle& style);
bool isBreakSpace(uint32_t codepoint);

}   // namespace TextMetrics

}   // namespace Engine
