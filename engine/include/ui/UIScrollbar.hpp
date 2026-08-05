#pragma once

#include "styling/UILayoutConfig.hpp"
#include "graphics/Color.hpp"
#include "utils/math/Vec2.hpp"
#include "utils/math/Vec4.hpp"

namespace Engine {

struct UILayoutNode;

struct UIScrollbarStyle
{
    Color trackColor = Color(0.0f, 0.0f, 0.0f, 0.22f);
    Color trackBorderColor = Color(1.0f, 1.0f, 1.0f, 0.14f);
    Color thumbColor = Color(1.0f, 1.0f, 1.0f, 0.34f);
    Color thumbBorderColor = Color(1.0f, 1.0f, 1.0f, 0.50f);
    Color thumbHoverColor = Color(1.0f, 1.0f, 1.0f, 0.58f);
    Color thumbHoverBorderColor = Color(1.0f, 1.0f, 1.0f, 0.90f);
    float thickness = 12.0f;
    float minThumbLength = 28.0f;
    float radius = 6.0f;
    float borderWidth = 1.0f;
    // Holds the thumb clear of the track's own outline, so the two rings read as a
    // handle sitting in a trough instead of one muddy doubled edge.
    float thumbInset = 2.0f;
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
    float borderWidth = 0.0f;
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
