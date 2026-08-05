#include "graphics/ui/theme/UIThemeColors.hpp"

namespace Engine {

namespace {

const Color SEED_BACKGROUND(0.16f, 0.17f, 0.22f);
const Color SEED_ACCENT(0.30f, 0.62f, 0.95f);
const Color SEED_TEXT(0.90f, 0.92f, 0.96f);

Color mix(const Color& from, const Color& to, float t)
{
    return Color(
        from.r + (to.r - from.r) * t, from.g + (to.g - from.g) * t, from.b + (to.b - from.b) * t,
        from.a + (to.a - from.a) * t
    );
}

Color lighten(const Color& color, float amount) { return mix(color, COLOR_WHITE, amount); }
Color darken(const Color& color, float amount) { return mix(color, COLOR_BLACK, amount); }

// Rec. 709, so a yellow accent reads as light and takes dark text on top of it while a
// blue one of the same RGB average does not.
float luminance(const Color& color)
{
    return 0.2126f * color.r + 0.7152f * color.g + 0.0722f * color.b;
}

}   // namespace

UIThemeColors UIThemeColors::resolve(const UIThemeColorsSpec& spec)
{
    UIThemeColors colors;
    colors.custom = spec.custom;

    colors.background = spec.background.value_or(SEED_BACKGROUND);
    colors.accent = spec.accent.value_or(SEED_ACCENT);
    colors.text = spec.text.value_or(SEED_TEXT);

    const Color& bg = colors.background;
    const Color& accent = colors.accent;
    const Color& text = colors.text;

    // Every surface derives *away* from the background, so the direction flips on a light
    // theme -- lightening a near-white panel to raise it produces no contrast at all.
    // Inputs sit above the panel rather than below it, which is why sunken moves the same
    // way as raised, only less far.
    float step = luminance(bg) > 0.5f ? -1.0f : 1.0f;
    auto shift = [&](float amount) {
        return amount * step > 0.0f ? lighten(bg, amount * step) : darken(bg, -amount * step);
    };

    colors.surfaceRaised = spec.surfaceRaised.value_or(shift(0.08f));
    colors.surfaceSunken = spec.surfaceSunken.value_or(shift(0.04f));
    colors.surfaceOverlay = spec.surfaceOverlay.value_or(shift(-0.12f));
    colors.surfaceHover = spec.surfaceHover.value_or(shift(0.10f));
    colors.surfaceActive = spec.surfaceActive.value_or(shift(0.14f));

    colors.accentHover = spec.accentHover.value_or(lighten(accent, 0.12f));
    colors.accentActive = spec.accentActive.value_or(darken(accent, 0.18f));
    colors.accentMuted = spec.accentMuted.value_or(mix(accent, bg, 0.55f));
    colors.onAccent =
        spec.onAccent.value_or(luminance(accent) > 0.6f ? darken(bg, 0.3f) : COLOR_WHITE);

    colors.border = spec.border.value_or(mix(bg, text, 0.16f));
    colors.borderStrong = spec.borderStrong.value_or(mix(bg, text, 0.30f));
    colors.borderFocus = spec.borderFocus.value_or(accent);

    colors.textMuted = spec.textMuted.value_or(mix(text, bg, 0.28f));
    colors.textSubtle = spec.textSubtle.value_or(mix(text, bg, 0.42f));

    // Fixed rather than derived: status colours carry meaning, and deriving a "red" from
    // a red accent would make danger invisible against it.
    colors.success = spec.success.value_or(Color(0.35f, 0.78f, 0.45f));
    colors.warning = spec.warning.value_or(Color(0.95f, 0.72f, 0.25f));
    colors.danger = spec.danger.value_or(Color(0.92f, 0.35f, 0.38f));

    colors.shadow = spec.shadow.value_or(Color(0.0f, 0.0f, 0.0f, 0.7f));
    return colors;
}

Color UIThemeColors::get(std::string_view name, Color fallback) const
{
    auto found = custom.find(std::string(name));
    return found == custom.end() ? fallback : found->second;
}

}   // namespace Engine
