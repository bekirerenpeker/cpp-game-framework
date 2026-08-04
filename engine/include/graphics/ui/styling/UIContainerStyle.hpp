#pragma once

#include "graphics/Color.hpp"
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

// Deliberately not UIEdges: its four names are left/right/top/bottom, which would have
// to be read as corners to be useful here. The single-float constructor keeps the
// common uniform case a plain `.borderRadius = 6.0f`.
struct UICorners
{
    float topLeft = 0.0f;
    float topRight = 0.0f;
    float bottomRight = 0.0f;
    float bottomLeft = 0.0f;

    UICorners() = default;
    UICorners(float all) : topLeft(all), topRight(all), bottomRight(all), bottomLeft(all) {}
    UICorners(float top, float bottom)
        : topLeft(top), topRight(top), bottomRight(bottom), bottomLeft(bottom)
    {
    }
    UICorners(float topLeft, float topRight, float bottomRight, float bottomLeft)
        : topLeft(topLeft), topRight(topRight), bottomRight(bottomRight), bottomLeft(bottomLeft)
    {
    }
};

// The field list lives once, here, and every struct that carries a container's look is
// generated from it -- adding a field is one line and both structs follow. The third
// column is the plain, unthemed fallback fillDefaults() uses: never a themed colour,
// just "don't show this" (CLEAR / zero) or the most neutral concrete value.
// std::optional<Gradient> backgroundGradient belongs in here once gradients exist.
#define UI_CONTAINER_STYLE_FIELDS(X)                                                               \
    X(Color, backgroundColor, COLOR_CLEAR)                                                         \
    X(GlTexture*, backgroundImage, nullptr)                                                        \
    X(float, imageRotation, 0.0f)                                                                  \
    X(Color, borderColor, COLOR_CLEAR)                                                             \
    X(float, borderWidth, 0.0f)                                                                    \
    X(UICorners, borderRadius, UICorners {})                                                       \
    X(UIBorderStyle, borderStyle, UIBorderStyle::Solid)                                            \
    X(Color, shadowColor, COLOR_CLEAR)                                                             \
    X(Vec2, shadowOffset, VEC2_ZERO)                                                               \
    X(float, shadowBlurRadius, 0.0f)                                                               \
    X(UIOverflow, overflow, UIOverflow::Visible)                                                   \
    X(bool, ignoreClip, false)                                                                     \
    X(bool, ignoreInput, false)                                                                    \
    X(int, zIndex, 0)                                                                              \
    X(UICursor, cursor, UICursor::Default)                                                         \
    X(float, transitionDuration, 0.0f)                                                             \
    X(UITransition, transition, UITransition::Linear)

#define UI_STYLE_DECLARE_FIELD(type, name, def) std::optional<type> name;
#define UI_STYLE_MERGE_FIELD(type, name, def)                                                      \
    if (other.name) name = other.name;
#define UI_STYLE_COPY_FIELD(type, name, def) style.name = name;
#define UI_STYLE_FILL_DEFAULT_FIELD(type, name, def)                                               \
    if (!name) name = def;

struct UIContainerStyle
{
    UI_CONTAINER_STYLE_FIELDS(UI_STYLE_DECLARE_FIELD)

    UIContainerStyle& combine(const UIContainerStyle& other)
    {
        UI_CONTAINER_STYLE_FIELDS(UI_STYLE_MERGE_FIELD)
        return *this;
    }

    UIContainerStyle combined(const UIContainerStyle& other) const
    {
        UIContainerStyle result = *this;
        result.combine(other);
        return result;
    }

    // Guarantees every field holds a value regardless of what a theme (or a caller)
    // left unset -- fields already set are untouched. The values that get filled in
    // carry no theming of their own, so a style with nothing configured stays inert
    // (invisible background/border/shadow) rather than acquiring an opinionated look.
    UIContainerStyle& fillDefaults()
    {
        // Ahead of the generic fill, whose CLEAR would win the !backgroundColor test
        // below it: an image with no colour of its own draws untinted, so white is its
        // plain default, while a container with neither stays invisible.
        if (!backgroundColor && backgroundImage.value_or(nullptr)) backgroundColor = COLOR_WHITE;

        UI_CONTAINER_STYLE_FIELDS(UI_STYLE_FILL_DEFAULT_FIELD)
        return *this;
    }
};

// Every field of UIContainerStyle plus one override per input state, so a container's
// whole reactive look is declared in the call that creates it. The fields are repeated
// via the macro instead of inherited, because a designated initializer only names a
// *direct* member -- `{.backgroundColor = ...}` would stop compiling if these lived in
// a base class.
struct UIContainerStyleSpec
{
    UI_CONTAINER_STYLE_FIELDS(UI_STYLE_DECLARE_FIELD)

    UIContainerStyle onHover;
    UIContainerStyle onHeld;
    UIContainerStyle onPressed;
    UIContainerStyle onReleased;

    UIContainerStyle base() const
    {
        UIContainerStyle style;
        UI_CONTAINER_STYLE_FIELDS(UI_STYLE_COPY_FIELD)
        return style;
    }

    UIContainerStyleSpec& combine(const UIContainerStyleSpec& other)
    {
        UI_CONTAINER_STYLE_FIELDS(UI_STYLE_MERGE_FIELD)
        onHover.combine(other.onHover);
        onHeld.combine(other.onHeld);
        onPressed.combine(other.onPressed);
        onReleased.combine(other.onReleased);
        return *this;
    }

    UIContainerStyleSpec combined(const UIContainerStyleSpec& other) const
    {
        UIContainerStyleSpec result = *this;
        result.combine(other);
        return result;
    }
};

}   // namespace Engine
