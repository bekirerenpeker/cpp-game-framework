#pragma once

#include "graphics/Color.hpp"
#include "graphics/text/Font.hpp"
#include "graphics/text/TextStyle.hpp"
#include "graphics/gl_wrappers/GlTexture.hpp"
#include "utils/math/Vec2.hpp"
#include <optional>

namespace Engine {

enum class UIBorderStyle
{
    Solid,
    Dashed,
    Dotted
};

enum class UIOverflow
{
    Visible,
    Hidden,
    Scroll
};

enum class UICursor
{
    Default,
    Pointer,
    Text,
    Move,
    Crosshair,
    NotAllowed
};

enum class UITransition
{
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut
};

struct UIContainerStyle
{
    std::optional<Color> backgroundColor;
    // std::optional<Gradient> backgroundGradient;
    std::optional<GlTexture*> backgroundImage;

    std::optional<Color> borderColor;
    std::optional<float> borderWidth;
    std::optional<float> borderRadius;
    std::optional<UIBorderStyle> borderStyle;

    std::optional<Color> shadowColor;
    std::optional<Vec2> shadowOffset;
    std::optional<float> shadowBlurRadius;

    std::optional<UIOverflow> overflow;
    std::optional<int> zIndex;
    std::optional<UICursor> cursor;

    std::optional<Vec2> offset;
    std::optional<Vec2> scale;

    std::optional<float> transitionDuration;
    std::optional<UITransition> transition;

    void combine(const UIContainerStyle& other)
    {
        if (other.backgroundColor) backgroundColor = other.backgroundColor;
        if (other.backgroundImage) backgroundImage = other.backgroundImage;

        if (other.borderColor) borderColor = other.borderColor;
        if (other.borderWidth) borderWidth = other.borderWidth;
        if (other.borderRadius) borderRadius = other.borderRadius;
        if (other.borderStyle) borderStyle = other.borderStyle;

        if (other.shadowColor) shadowColor = other.shadowColor;
        if (other.shadowOffset) shadowOffset = other.shadowOffset;
        if (other.shadowBlurRadius) shadowBlurRadius = other.shadowBlurRadius;

        if (other.overflow) overflow = other.overflow;
        if (other.zIndex) zIndex = other.zIndex;
        if (other.cursor) cursor = other.cursor;

        if (other.offset) offset = other.offset;
        if (other.scale) scale = other.scale;

        if (other.transitionDuration) transitionDuration = other.transitionDuration;
        if (other.transition) transition = other.transition;
    }
};

}   // namespace Engine
