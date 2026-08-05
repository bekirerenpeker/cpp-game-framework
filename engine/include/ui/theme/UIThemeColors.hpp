#pragma once

#include "graphics/Color.hpp"
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Engine {

// Every role a widget is allowed to name. Three of them -- background, accent, text --
// are seeds: set only those and resolve() derives the rest by lightening, darkening and
// mixing, so a whole retheme is three lines. Any role set explicitly is left alone.
#define UI_THEME_COLOR_FIELDS(X)                                                                   \
    X(background)                                                                                  \
    X(surfaceRaised)                                                                               \
    X(surfaceSunken)                                                                               \
    X(surfaceOverlay)                                                                              \
    X(surfaceHover)                                                                                \
    X(surfaceActive)                                                                               \
    X(accent)                                                                                      \
    X(accentHover)                                                                                 \
    X(accentActive)                                                                                \
    X(accentMuted)                                                                                 \
    X(onAccent)                                                                                    \
    X(border)                                                                                      \
    X(borderStrong)                                                                                \
    X(borderFocus)                                                                                 \
    X(foreground)                                                                                        \
    X(foregroundMuted)                                                                                   \
    X(foregroundSubtle)                                                                                  \
    X(success)                                                                                     \
    X(warning)                                                                                     \
    X(danger)                                                                                      \
    X(shadow)

#define UI_THEME_COLOR_DECLARE_OPTIONAL(name) std::optional<Color> name;
#define UI_THEME_COLOR_DECLARE_PLAIN(name)    Color name;
#define UI_THEME_COLOR_MERGE(name)                                                                 \
    if (other.name) name = other.name;

// What a caller writes. Optional for the same reason every other UI struct is: resolve()
// has to tell "left alone, derive it" apart from "set to exactly this".
struct UIThemeColorsSpec
{
    UI_THEME_COLOR_FIELDS(UI_THEME_COLOR_DECLARE_OPTIONAL)

    std::unordered_map<std::string, Color> custom;

    UIThemeColorsSpec& combine(const UIThemeColorsSpec& other)
    {
        UI_THEME_COLOR_FIELDS(UI_THEME_COLOR_MERGE)
        for (const auto& [name, color] : other.custom) custom[name] = color;
        return *this;
    }
};

// What a widget reads: every role concrete, so a call site is `colors.accent`, not a
// dereference. Only UIThemeManager produces one.
struct UIThemeColors
{
    UI_THEME_COLOR_FIELDS(UI_THEME_COLOR_DECLARE_PLAIN)

    std::unordered_map<std::string, Color> custom;

    static UIThemeColors resolve(const UIThemeColorsSpec& spec);

    Color get(std::string_view name, Color fallback = COLOR_CLEAR) const;
};

}   // namespace Engine
