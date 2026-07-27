#pragma once

#include "graphics/Color.hpp"
#include "utils/math/Vec2.hpp"

namespace Engine {

// One struct for every layer of the text API, and for both atlas types. size is
// an em size in the target space's units (pixels for a screen-space pass, world
// units for a world-space one). Every other measurement is in em, so the whole
// style scales with size and stays put under a camera zoom.
// outlineWidth, boldness, shadowWidth and the softnesses are capped by what the
// atlas baked: see Font::getMaxEffectEm(), and raise
// FontBakeSettings::distanceRangePixels for wider effects.
// A zero shadowOffset turns the shadow into a centred glow.
// A Bitmap font keeps colour, spacing, italicSkew, underline and strikethrough
// but ignores everything distance-field based: outline, boldness, softness, shadow.
struct TextStyle
{
    Color color = COLOR_WHITE;
    float size = 32.0f;
    float letterSpacing = 0.0f;
    float lineSpacing = 1.0f;

    Color outlineColor = COLOR_BLACK;
    float outlineWidth = 0.0f;
    float boldness = 0.0f;
    float softness = 0.0f;

    Color shadowColor = COLOR_BLACK;
    float shadowWidth = 0.0f;
    float shadowSoftness = 0.0f;
    Vec2 shadowOffset = VEC2_ZERO;

    float italicSkew = 0.0f;
    bool underline = false;
    bool strikethrough = false;
};

}   // namespace Engine
