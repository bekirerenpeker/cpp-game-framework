#pragma once

#include "graphics/ui/UiManager.hpp"
#include <string>
#include <vector>

namespace Engine {

namespace UIWidgets {

// base components interfaces
void clear();
UINode* getNode(IdType id);

UINodeState openContainer(
    const UILayoutConfig& layout = {}, const UIContainerStyleSpec& style = {},
    std::string_view key = {}
);
void closeContainer();

IdType addTextLeaf(const UILayoutConfig& layout, const UITextConfig& config);
IdType addShaderLeaf(const UILayoutConfig& layout, const UIShaderConfig& config);

// composite widgets
struct DragHandleConfig
{
    UILayoutConfig handleLayout;
    UIContainerStyleSpec handleStyle;
    std::string_view key;
};
UINodeState dragHandle(const DragHandleConfig& config = {});

struct WindowConfig
{
    UILayoutConfig windowLayout;
    UIContainerStyleSpec windowStyle;
    UILayoutConfig titleLayout;
    UIContainerStyleSpec titleStyle;
    UITextConfig titleTextConfig;
    UILayoutConfig bodyLayout;
    UIContainerStyleSpec bodyStyle;
    std::string_view key;
};
UINodeState openWindow(const std::string& name, const WindowConfig& config = {});
void closeWindow();

struct ButtonConfig
{
    UILayoutConfig buttonLayout;
    UIContainerStyleSpec buttonStyle;
    UILayoutConfig textLayout;
    UITextConfig textConfig;
    std::string_view key;
};
UINodeState button(const std::string text, const ButtonConfig& config = {});

struct DividerConfig
{
    UILayoutConfig dividerLayout;
    UIContainerStyleSpec dividerStyle;
    std::string_view key;
};
UINodeState horizontalDivider(const DividerConfig& config = {});
UINodeState verticalDivider(const DividerConfig& config = {});

struct ToolbarMenuConfig
{
    UILayoutConfig menuLayout;
    UIContainerStyleSpec menuStyle;
    UILayoutConfig itemLayout;
    UIContainerStyleSpec itemStyle;
    UIContainerStyleSpec selectedItemStyle;
    UILayoutConfig itemTextLayout;
    UITextConfig itemTextConfig;
    UITextStyle hoveredItemTextStyle;
    UITextStyle selectedItemTextStyle;
    std::string_view key;
};
void toolbarMenu(
    const std::vector<std::string>& items, int& selected, const ToolbarMenuConfig& config = {}
);

struct SliderConfig
{
    UILayoutConfig sliderLayout;
    UIContainerStyleSpec sliderStyle;
    UILayoutConfig trackLayout;
    UIContainerStyleSpec trackStyle;
    UIContainerStyleSpec fillStyle;
    UILayoutConfig handleLayout;
    UIContainerStyleSpec handleStyle;
    UILayoutConfig labelLayout;
    UITextConfig labelTextConfig;
    float step = 0.0f;
    int decimals = 2;
    std::string_view key;
};
void sliderFloat(
    const std::string label, float& value, float minValue, float maxValue,
    const SliderConfig& config = {}
);
void sliderInt(
    const std::string label, int& value, int minValue, int maxValueExcluded,
    const SliderConfig& config = {}
);

struct TextConfig
{
    UILayoutConfig textLayout;
    UITextConfig textConfig;
};
IdType text(const std::string label, const TextConfig& config = {});

struct CheckBoxConfig
{
    UILayoutConfig rowLayout;
    UIContainerStyleSpec rowStyle;
    UILayoutConfig boxLayout;
    UIContainerStyleSpec boxStyle;
    UIContainerStyleSpec checkedBoxStyle;
    UILayoutConfig markLayout;
    UIContainerStyleSpec markStyle;
    UILayoutConfig labelLayout;
    UITextConfig labelTextConfig;
    std::string_view key;
};
UINodeState checkBox(const std::string label, bool& checked, const CheckBoxConfig& config = {});

struct RadioGroupConfig
{
    UILayoutConfig groupLayout;
    UIContainerStyleSpec groupStyle;
    UILayoutConfig rowLayout;
    UIContainerStyleSpec rowStyle;
    UILayoutConfig dotLayout;
    UIContainerStyleSpec dotStyle;
    UIContainerStyleSpec selectedDotStyle;
    UILayoutConfig labelLayout;
    UITextConfig labelTextConfig;
    UITextStyle selectedLabelTextStyle;
    std::string_view key;
};
void radioGroup(
    const std::vector<std::string>& items, int& selected, const RadioGroupConfig& config = {}
);

struct TooltipConfig
{
    UILayoutConfig tooltipLayout;
    UIContainerStyleSpec tooltipStyle;
    UILayoutConfig labelLayout;
    UITextConfig labelTextConfig;
    Vec2 cursorOffset = Vec2(14.0f, 18.0f);
    std::string_view key;
};
void tooltip(const std::string label, bool visible, const TooltipConfig& config = {});

struct ColorPickerConfig
{
    UILayoutConfig pickerLayout;
    UIContainerStyleSpec pickerStyle;
    UILayoutConfig squareLayout;
    UILayoutConfig hueLayout;
    UILayoutConfig markerLayout;
    UIContainerStyleSpec markerStyle;
    UILayoutConfig hueMarkerLayout;
    UIContainerStyleSpec hueMarkerStyle;
    UILayoutConfig previewLayout;
    UIContainerStyleSpec previewStyle;
    float squareHeight = 120.0f;
    float hueWidth = 18.0f;
    std::string_view key;
};
void colorPicker(Color& color, const ColorPickerConfig& config = {});

}   // namespace UIWidgets

}   // namespace Engine
