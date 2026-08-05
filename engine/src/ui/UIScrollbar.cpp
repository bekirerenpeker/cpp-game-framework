#include "ui/UIScrollbar.hpp"
#include "ui/UILayoutCalculator.hpp"
#include "ui/UIRenderer.hpp"
#include "utils/math/MathFuncs.hpp"

namespace Engine {

namespace {

// Capped at half the short side, since a radius wider than the box it rounds leaves the
// SDF describing something the box cannot be.
UIContainerStyle barQuad(Color fill, Color border, float borderWidth, float radius, Vec2 size)
{
    UIContainerStyle style;
    style.fillDefaults();
    style.backgroundColor = fill;
    style.borderColor = border;
    style.borderWidth = borderWidth;
    style.borderRadius = UICorners(Math::min(radius, Math::min(size.x, size.y) * 0.5f));
    return style;
}

}   // namespace

UIScrollbar
scrollbarOf(const UILayoutNode& node, UILayoutAxis axis, const UIScrollbarStyle& style, float scale)
{
    UIScrollbar bar;

    float view = axisGet(node.size, axis);
    float content = axisGet(node.contentSize, axis);
    bar.range = content - view;
    if (bar.range <= 0.0f || view <= 0.0f) return bar;

    float thickness = style.thickness * scale;
    Vec2 half = node.size * 0.5f;

    // Each track spans its whole edge, so with both bars up they cross in the corner --
    // cheaper to live with than making either axis's length depend on whether the other
    // one happens to be showing this frame.
    if (axis == UILayoutAxis::Vertical) {
        bar.trackSize = Vec2(thickness, node.size.y);
        bar.trackPos = Vec2(node.drawPos.x + half.x - thickness * 0.5f, node.drawPos.y);
    } else {
        bar.trackSize = Vec2(node.size.x, thickness);
        bar.trackPos = Vec2(node.drawPos.x, node.drawPos.y - half.y + thickness * 0.5f);
    }

    float trackLength = axisGet(bar.trackSize, axis);
    float minLength = Math::min(style.minThumbLength * scale, trackLength);
    float thumbLength = Math::max(trackLength * (view / content), minLength);

    bar.travel = trackLength - thumbLength;
    bar.radius = style.radius * scale;
    bar.borderWidth = style.borderWidth * scale;

    float thumbThickness = Math::max(thickness - 2.0f * style.thumbInset * scale, 1.0f);
    float t = Math::clamp(axisGet(node.scroll, axis) / bar.range, 0.0f, 1.0f);

    // Scroll runs y-down while draw space is y-up, so the vertical thumb walks *down*
    // from the top of its track while the horizontal one walks right from the left.
    if (axis == UILayoutAxis::Vertical) {
        bar.thumbSize = Vec2(thumbThickness, thumbLength);
        bar.thumbPos = Vec2(
            bar.trackPos.x,
            bar.trackPos.y + trackLength * 0.5f - t * bar.travel - thumbLength * 0.5f
        );
    } else {
        bar.thumbSize = Vec2(thumbLength, thumbThickness);
        bar.thumbPos = Vec2(
            bar.trackPos.x - trackLength * 0.5f + t * bar.travel + thumbLength * 0.5f,
            bar.trackPos.y
        );
    }

    bar.isVisible = true;
    return bar;
}

bool scrollbarThumbContains(const UIScrollbar& bar, Vec2 point)
{
    if (!bar.isVisible) return false;

    Vec2 half = bar.thumbSize * 0.5f;
    Vec2 delta = point - bar.thumbPos;
    return Math::abs(delta.x) <= half.x && Math::abs(delta.y) <= half.y;
}

void drawScrollbar(
    const UIScrollbar& bar, const UIScrollbarStyle& style, bool thumbHovered, Vec4 clipRect
)
{
    if (!bar.isVisible) return;

    UIRenderer& renderer = UIRenderer::get();
    renderer.addContainerQuad(
        bar.trackPos, bar.trackSize,
        barQuad(
            style.trackColor, style.trackBorderColor, bar.borderWidth, bar.radius, bar.trackSize
        ),
        clipRect
    );
    renderer.addContainerQuad(
        bar.thumbPos, bar.thumbSize,
        barQuad(
            thumbHovered ? style.thumbHoverColor : style.thumbColor,
            thumbHovered ? style.thumbHoverBorderColor : style.thumbBorderColor, bar.borderWidth,
            bar.radius, bar.thumbSize
        ),
        clipRect
    );
}

}   // namespace Engine
