#pragma once

namespace Engine {

struct UIThemeSpacing
{
    float xs = 4.0f;
    float sm = 6.0f;
    float md = 10.0f;
    float lg = 16.0f;
    float xl = 24.0f;
};

struct UIThemeRadius
{
    float sm = 4.0f;
    float md = 6.0f;
    float lg = 9.0f;
    float pill = 9999.0f;   // clamped to half the shorter side by the renderer
};

struct UIThemeBorderWidth
{
    float thin = 1.0f;
    float thick = 2.0f;
};

// scale is the one field the UI system reads for itself: UIManager multiplies every
// pixel-valued layout and style field by it as a node is created, so layout output stays
// in real window pixels and hit testing, clip rects and the mouse need no changes at all.
// Scaling the projection instead would break every one of those.
struct UIThemeMetrics
{
    float scale = 1.0f;

    UIThemeSpacing spacing;
    UIThemeRadius radius;
    UIThemeBorderWidth borderWidth;

    float controlHeight = 28.0f;
    float controlHeightSmall = 20.0f;
    float iconSize = 16.0f;
};

}   // namespace Engine
