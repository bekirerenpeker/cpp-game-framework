#pragma once

#include "UITheme.hpp"
#include <string>
#include <vector>

namespace Engine {

struct UIThemePreset
{
    std::string name;
    UITheme theme;
};

// Built into the engine rather than left to each game: a preset is only a handful of
// seeds, and every one of them would otherwise be reinvented. Most set the three seeds
// and let the rest derive; the ones that name more do it because the look depends on it
// (Brutalist's square corners and heavy rules, Amber CRT's tinted border).
const std::vector<UIThemePreset>& getThemePresets();

std::vector<std::string> getThemePresetNames();

}   // namespace Engine
