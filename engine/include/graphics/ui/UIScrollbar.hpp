#pragma once

#include "graphics/Color.hpp"
#include "graphics/ui/styling/UILayoutConfig.hpp"
#include "utils/math/Vec2.hpp"
#include "utils/math/Vec4.hpp"

namespace Engine {

struct UILayoutNode;

struct UIScrollbarStyle
{
    Color trackColor = Color(0.0f, 0.0f, 0.0f, 0.16f);
    Color thumbColor = Color(1.0f, 1.0f, 1.0f, 0.26f);
    Color thumbHoverColor = Color(1.0f, 1.0f, 1.0f, 0.50f);
    float thickness = 8.0f;
    float minThumbLength = 24.0f;
    float radius = 4.0f;
};

// A bar is not a node: it is derived from a solved UILayoutNode every frame, so it needs
// no place in the tree, no key of its own and no frame of lag. Positions are draw space
// (a centre, y-up), which is what UIRenderer::addContainerQuad takes. travel is the
// pixels the thumb may slide and range the scroll those pixels map onto, so a drag
// converts between them without re-deriving any of this.
struct UIScrollbar
{
    bool isVisible = false;
    Vec2 trackPos = VEC2_ZERO;
    Vec2 trackSize = VEC2_ZERO;
    Vec2 thumbPos = VEC2_ZERO;
    Vec2 thumbSize = VEC2_ZERO;
    float radius = 0.0f;
    float travel = 0.0f;
    float range = 0.0f;
};

UIScrollbar scrollbarOf(
    const UILayoutNode& node, UILayoutAxis axis, const UIScrollbarStyle& style, float scale
);
bool scrollbarThumbContains(const UIScrollbar& bar, Vec2 point);
void drawScrollbar(
    const UIScrollbar& bar, const UIScrollbarStyle& style, bool thumbHovered, Vec4 clipRect
);

}   // namespace Engine
