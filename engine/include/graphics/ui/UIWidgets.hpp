#pragma once

#include "graphics/ui/UiManager.hpp"

namespace Engine {

namespace UIWidgets {

// base components interfaces
UINodeState openContainer(
    const UILayoutConfig& layout = {}, const UIContainerStyleSpec& style = {},
    std::string_view key = {}
);
void closeContainer();

IdType addTextLeaf(const UILayoutConfig& layout, const UITextConfig& config);

// composite widgets
struct ButtonConfig
{
    UILayoutConfig buttonLayout;
    UIContainerStyleSpec buttonStyle;
    UILayoutConfig textLayout;
    UITextConfig textConfig;
};
UINodeState
button(const std::string text, const ButtonConfig& config = {}, std::string_view key = {});

}   // namespace UIWidgets

}   // namespace Engine
