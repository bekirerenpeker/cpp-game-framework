#include "ui/theme/UIThemePresets.hpp"

namespace Engine {

namespace {

std::vector<UIThemePreset> buildPresets()
{
    std::vector<UIThemePreset> presets;

    presets.push_back(
        {"Midnight",
         {.colors = {
              .background = Color(0.16f, 0.17f, 0.22f),
              .accent = Color(0.30f, 0.62f, 0.95f),
              .foreground = Color(0.90f, 0.92f, 0.96f),
          }}}
    );

    // Square corners and a border tinted toward the phosphor, which is what sells it --
    // the seeds alone would read as "orange theme" rather than as a terminal.
    UITheme amber;
    amber.colors.background = Color(0.06f, 0.05f, 0.04f);
    amber.colors.accent = Color(0.98f, 0.68f, 0.15f);
    amber.colors.foreground = Color(0.96f, 0.80f, 0.45f);
    amber.colors.border = Color(0.42f, 0.28f, 0.08f);
    amber.metrics.radius = {1.0f, 1.0f, 2.0f, 2.0f};
    presets.push_back({"Amber CRT", amber});

    presets.push_back(
        {"Deep Sea",
         {.colors = {
              .background = Color(0.05f, 0.12f, 0.16f),
              .accent = Color(0.16f, 0.84f, 0.78f),
              .foreground = Color(0.84f, 0.95f, 0.96f),
          }}}
    );

    presets.push_back(
        {"Forest Floor",
         {.colors = {
              .background = Color(0.10f, 0.14f, 0.10f),
              .accent = Color(0.56f, 0.80f, 0.34f),
              .foreground = Color(0.90f, 0.94f, 0.85f),
          }}}
    );

    // Fully round controls, so the same widgets read as soft rather than technical.
    UITheme vapor;
    vapor.colors.background = Color(0.13f, 0.07f, 0.20f);
    vapor.colors.accent = Color(0.96f, 0.36f, 0.76f);
    vapor.colors.foreground = Color(0.93f, 0.88f, 0.99f);
    vapor.colors.custom["secondary"] = Color(0.35f, 0.90f, 0.95f);
    vapor.metrics.radius = {8.0f, 12.0f, 16.0f, 9999.0f};
    presets.push_back({"Vaporwave", vapor});

    // The light one, and the reason surface derivation had to learn to darken instead of
    // lighten: raising a near-white panel by lightening it produces nothing.
    presets.push_back(
        {"Rosewater",
         {.colors = {
              .background = Color(0.95f, 0.93f, 0.92f),
              .accent = Color(0.78f, 0.28f, 0.42f),
              .foreground = Color(0.16f, 0.13f, 0.15f),
          }}}
    );

    // No radius anywhere and rules thick enough to be the design. Every widget picks this
    // up for free because both are ramp values rather than per-widget literals.
    UITheme brutal;
    brutal.colors.background = Color(0.08f, 0.08f, 0.08f);
    brutal.colors.accent = Color(1.0f, 0.26f, 0.10f);
    brutal.colors.foreground = Color(0.98f, 0.98f, 0.98f);
    brutal.colors.border = Color(0.98f, 0.98f, 0.98f);
    brutal.colors.borderStrong = Color(0.98f, 0.98f, 0.98f);
    brutal.metrics.radius = {0.0f, 0.0f, 0.0f, 0.0f};
    brutal.metrics.borderWidth = {2.0f, 4.0f};
    brutal.textStyles["h1"] = {.size = 24.0f};
    presets.push_back({"Brutalist", brutal});

    return presets;
}

}   // namespace

const std::vector<UIThemePreset>& getThemePresets()
{
    static const std::vector<UIThemePreset> presets = buildPresets();
    return presets;
}

std::vector<std::string> getThemePresetNames()
{
    std::vector<std::string> names;
    names.reserve(getThemePresets().size());
    for (const UIThemePreset& preset : getThemePresets()) names.push_back(preset.name);
    return names;
}

}   // namespace Engine
