#pragma once

#include "graphics/Color.hpp"
#include "graphics/text/TextStyle.hpp"
#include "graphics/ui/layout/ILeafMeasurer.hpp"
#include "graphics/ui/layout/LayoutTypes.hpp"
#include <cstdint>
#include <optional>
#include <string>

namespace Engine {

class Font;
class GlTexture;

enum class UiElementType : uint8_t
{
    Container = 0,
    Text,
    Button,
    Image,
};

struct UiStyle
{
    Color backgroundColor = COLOR_CLEAR;
    Color borderColor = COLOR_CLEAR;
    Color contentColor = COLOR_WHITE;
    float borderWidth = 0.0f;
    float cornerRadius = 0.0f;
};

struct UiStyleOverride
{
    std::optional<Color> backgroundColor;
    std::optional<Color> borderColor;
    std::optional<Color> contentColor;
    std::optional<float> borderWidth;

    void applyTo(UiStyle& style) const
    {
        if (backgroundColor) style.backgroundColor = *backgroundColor;
        if (borderColor) style.borderColor = *borderColor;
        if (contentColor) style.contentColor = *contentColor;
        if (borderWidth) style.borderWidth = *borderWidth;
    }
};

struct UiStyles
{
    UiStyle normal;
    UiStyleOverride hovered;
    UiStyleOverride pressed;

    UiStyles() = default;
    UiStyles(const UiStyle& style) : normal(style) {}
};

// Starting points, not a theme -- copy one and edit the fields you care about. A
// default-constructed UiStyles already draws nothing, which is what a pure layout
// container wants, so there is no preset for that.
namespace UiPresets {

inline Color shifted(Color color, float amount)
{
    auto channel = [amount](float v) {
        return v + amount < 0.0f ? 0.0f : (v + amount > 1.0f ? 1.0f : v + amount);
    };
    return Color(channel(color.r), channel(color.g), channel(color.b), color.a);
}

inline UiStyles panel(Color tint = Color(0.14f, 0.16f, 0.20f))
{
    UiStyles styles;
    styles.normal.backgroundColor = tint;
    styles.normal.borderColor = shifted(tint, 0.14f);
    styles.normal.borderWidth = 1.0f;
    styles.normal.cornerRadius = 8.0f;
    return styles;
}

inline UiStyles button(Color tint = Color(0.20f, 0.23f, 0.30f))
{
    UiStyles styles;
    styles.normal.backgroundColor = tint;
    styles.normal.borderColor = shifted(tint, 0.16f);
    styles.normal.borderWidth = 1.0f;
    styles.normal.cornerRadius = 6.0f;
    styles.hovered.backgroundColor = shifted(tint, 0.09f);
    styles.hovered.borderColor = shifted(tint, 0.34f);
    styles.pressed.backgroundColor = shifted(tint, -0.09f);
    return styles;
}

inline UiStyles outline(Color tint = Color(0.45f, 0.50f, 0.60f))
{
    UiStyles styles;
    styles.normal.borderColor = tint;
    styles.normal.borderWidth = 1.0f;
    styles.normal.cornerRadius = 4.0f;
    return styles;
}

}   // namespace UiPresets

struct UiElement
{
    LayoutConfig layout;
    UiStyle style;
    TextStyle textStyle;

    std::string text;
    const Font* font = nullptr;
    const GlTexture* texture = nullptr;
    ILeafMeasurer* measurer = nullptr;

    uint parent = NO_NODE;
    UiElementType type = UiElementType::Container;
};

}   // namespace Engine
