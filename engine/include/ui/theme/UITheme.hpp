#pragma once

#include "ui/styling/UITextStyle.hpp"
#include "UIThemeColors.hpp"
#include "UIThemeMetrics.hpp"
#include <string>
#include <unordered_map>

namespace Engine {

// What a caller hands to UIThemeManager. Colours are a spec (unset roles get derived),
// metrics are concrete, and textStyles is merged over the built-in set rather than
// replacing it -- so naming "body" restyles it and naming "myCaption" adds one.
struct UITheme
{
    UIThemeColorsSpec colors;
    UIThemeMetrics metrics;
    std::unordered_map<std::string, UITextStyle> textStyles;
};

}   // namespace Engine
