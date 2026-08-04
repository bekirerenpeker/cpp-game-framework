#pragma once

#include "graphics/text/TextStyle.hpp"
#include <optional>

namespace Engine {

// Optional-field mirror of TextStyle, for the same reason UIContainerStyle mirrors a
// concrete style: a theme's baseline and a caller's per-widget override need to combine
// field by field, and only an unset field can mean "didn't touch this." TextStyle itself
// stays a plain value struct because it is also used by world-space text, which has no
// theme to merge against. The third column is fillDefaults()'s plain, unthemed fallback --
// TextStyle's own construction defaults, not a themed choice.
#define UI_TEXT_STYLE_FIELDS(X)                                                                    \
    X(Color, color, COLOR_WHITE)                                                                   \
    X(float, size, 32.0f)                                                                          \
    X(float, letterSpacing, 0.0f)                                                                  \
    X(float, lineSpacing, 1.0f)                                                                    \
    X(Color, outlineColor, COLOR_BLACK)                                                            \
    X(float, outlineWidth, 0.0f)                                                                   \
    X(float, boldness, 0.0f)                                                                       \
    X(float, softness, 0.0f)                                                                       \
    X(Color, shadowColor, COLOR_BLACK)                                                             \
    X(float, shadowWidth, 0.0f)                                                                    \
    X(float, shadowSoftness, 0.0f)                                                                 \
    X(Vec2, shadowOffset, VEC2_ZERO)                                                               \
    X(float, italicSkew, 0.0f)                                                                     \
    X(bool, underline, false)                                                                      \
    X(bool, strikethrough, false)

#define UI_TEXT_STYLE_DECLARE_FIELD(type, name, def) std::optional<type> name;
#define UI_TEXT_STYLE_MERGE_FIELD(type, name, def)                                                 \
    if (other.name) name = other.name;
#define UI_TEXT_STYLE_FILL_DEFAULT_FIELD(type, name, def)                                          \
    if (!name) name = def;
#define UI_TEXT_STYLE_ASSIGN_FIELD(type, name, def) result.name = *filled.name;

struct UITextStyle
{
    UI_TEXT_STYLE_FIELDS(UI_TEXT_STYLE_DECLARE_FIELD)

    UITextStyle& combine(const UITextStyle& other)
    {
        UI_TEXT_STYLE_FIELDS(UI_TEXT_STYLE_MERGE_FIELD)
        return *this;
    }

    UITextStyle combined(const UITextStyle& other) const
    {
        UITextStyle result = *this;
        result.combine(other);
        return result;
    }

    UITextStyle& fillDefaults()
    {
        UI_TEXT_STYLE_FIELDS(UI_TEXT_STYLE_FILL_DEFAULT_FIELD)
        return *this;
    }

    // The one crossing back into the concrete TextStyle the text path actually draws
    // with. Fills first, so every field is engaged and the reads below cannot be
    // anything but a plain dereference.
    TextStyle resolve() const
    {
        UITextStyle filled = *this;
        filled.fillDefaults();

        TextStyle result;
        UI_TEXT_STYLE_FIELDS(UI_TEXT_STYLE_ASSIGN_FIELD)
        return result;
    }
};

}   // namespace Engine
