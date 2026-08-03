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

// The field list lives once, here, and every struct that carries a container's look is
// generated from it -- adding a field is one line and both structs follow.
// std::optional<Gradient> backgroundGradient belongs in here once gradients exist.
#define UI_CONTAINER_STYLE_FIELDS(X)                                                               \
    X(Color, backgroundColor)                                                                      \
    X(GlTexture*, backgroundImage)                                                                 \
    X(Color, borderColor)                                                                          \
    X(float, borderWidth)                                                                          \
    X(float, borderRadius)                                                                         \
    X(UIBorderStyle, borderStyle)                                                                  \
    X(Color, shadowColor)                                                                          \
    X(Vec2, shadowOffset)                                                                          \
    X(float, shadowBlurRadius)                                                                     \
    X(UIOverflow, overflow)                                                                        \
    X(int, zIndex)                                                                                 \
    X(UICursor, cursor)                                                                            \
    X(float, transitionDuration)                                                                   \
    X(UITransition, transition)

#define UI_STYLE_DECLARE_FIELD(type, name) std::optional<type> name;
#define UI_STYLE_MERGE_FIELD(type, name)                                                           \
    if (other.name) name = other.name;
#define UI_STYLE_COPY_FIELD(type, name) style.name = name;

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
};

// Every field of UIContainerStyle plus one override per input state, so a container's
// whole reactive look is declared in the call that creates it.
//
// The fields are repeated through the macro instead of inherited from UIContainerStyle
// because a designated initializer may only name a *direct* member: the moment these
// live in a base class, `{.backgroundColor = ...}` stops compiling at every call site,
// and that spelling is the entire point of the struct.
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
};

}   // namespace Engine
