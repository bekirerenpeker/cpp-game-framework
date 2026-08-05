#pragma once

#include "graphics/ui/UiManager.hpp"
#include "graphics/ui/theme/UIThemeManager.hpp"
#include <string>
#include <vector>

namespace Engine {

namespace UIWidgets {

// What a widget with a bound value hands back. The value itself still travels through
// the reference the caller passed; this only says *when* to act on it. isChanged fires
// on every frame a drag moves the value and isReleased once as the gesture ends, which
// is the one an expensive update should hang off. A widget with no drag to it -- a
// checkbox, a dropdown item -- completes the instant it is clicked, so for those the two
// fire together and isEditing never does.
struct UIInputState
{
    UINodeState node;
    bool isChanged = false;
    bool isEditing = false;
    bool isReleased = false;
};

// base components interfaces
void clear();
UINode* getNode(IdType id);
void removeChildren(IdType id);

// A config field is in design units and gets multiplied by the theme scale on the way
// into the node; UINodeState hands back last frame's *solved* geometry, which is already
// in real pixels. Anything feeding solved geometry back into a config -- a popup offset
// off the anchor's height, a drag delta -- has to come back through here, or it scales
// twice.
float uiScale();
float unscale(float value);
Vec2 unscale(Vec2 value);

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

// Open/close like a window, and for the same reason: immediate mode gives a widget no
// way to stop its caller declaring content, so a collapsed section discards it in
// closeSection instead. The return value lets a caller skip building expensive content
// in the first place, but ignoring it stays correct.
struct SectionConfig
{
    UILayoutConfig sectionLayout;
    UIContainerStyleSpec sectionStyle;
    UILayoutConfig headerLayout;
    UIContainerStyleSpec headerStyle;
    UILayoutConfig arrowLayout;
    UIContainerStyleSpec arrowStyle;
    UITextConfig headerTextConfig;
    UILayoutConfig bodyLayout;
    UIContainerStyleSpec bodyStyle;
    bool openByDefault = false;
    std::string_view key;
};
bool openSection(const std::string& label, const SectionConfig& config = {});
void closeSection();

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
UIInputState toolbarMenu(
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
UIInputState sliderFloat(
    const std::string label, float& value, float minValue, float maxValue,
    const SliderConfig& config = {}
);
UIInputState sliderInt(
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
UIInputState checkBox(const std::string label, bool& checked, const CheckBoxConfig& config = {});

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
UIInputState radioGroup(
    const std::vector<std::string>& items, int& selected, const RadioGroupConfig& config = {}
);

struct DropdownConfig
{
    UILayoutConfig wrapperLayout;
    UILayoutConfig buttonLayout;
    UIContainerStyleSpec buttonStyle;
    UILayoutConfig labelLayout;
    UITextConfig labelTextConfig;
    UILayoutConfig arrowLayout;
    UIContainerStyleSpec arrowStyle;
    UILayoutConfig panelLayout;
    UIContainerStyleSpec panelStyle;
    UILayoutConfig itemLayout;
    UIContainerStyleSpec itemStyle;
    UIContainerStyleSpec selectedItemStyle;
    UILayoutConfig itemTextLayout;
    UITextConfig itemTextConfig;
    UITextStyle selectedItemTextStyle;
    // Shown on the closed dropdown instead of the selected item, which is what turns it
    // into a menu that always reads the same while still tracking a selection.
    std::string_view label;
    Vec2 panelOffset = Vec2(0.0f, 4.0f);
    std::string_view key;
};
UIInputState
dropdown(const std::vector<std::string>& items, int& selected, const DropdownConfig& config = {});

struct TooltipConfig
{
    UILayoutConfig tooltipLayout;
    UIContainerStyleSpec tooltipStyle;
    UILayoutConfig labelLayout;
    UITextConfig labelTextConfig;
    Vec2 cursorOffset = Vec2(14.0f, 18.0f);
    // Layout space, y-down -- a UINodeState::pos, optionally plus its size. Unset
    // follows the cursor instead.
    std::optional<Vec2> anchor;
    Vec2 anchorOffset = VEC2_ZERO;
    // Which point of the tooltip lands on the anchor: Start its top-left, Center its
    // middle, End its bottom-right.
    UIAlign anchorAlignX = UIAlign::Start;
    UIAlign anchorAlignY = UIAlign::Start;
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
    UILayoutConfig alphaLayout;
    UILayoutConfig alphaMarkerLayout;
    UIContainerStyleSpec alphaMarkerStyle;
    UILayoutConfig previewLayout;
    UIContainerStyleSpec previewStyle;
    UILayoutConfig valuesLayout;
    UITextConfig valuesTextConfig;
    float squareHeight = 120.0f;
    float hueWidth = 18.0f;
    float alphaHeight = 14.0f;
    bool showValues = true;
    std::string_view key;
};
UIInputState colorPicker(Color& color, const ColorPickerConfig& config = {});

struct ColorPickerPopupConfig
{
    UILayoutConfig wrapperLayout;
    UILayoutConfig swatchLayout;
    UIContainerStyleSpec swatchStyle;
    UILayoutConfig panelLayout;
    UIContainerStyleSpec panelStyle;
    ColorPickerConfig pickerConfig;
    Vec2 panelOffset = Vec2(0.0f, 4.0f);
    std::string_view key;
};
UIInputState colorPickerPopup(Color& color, const ColorPickerPopupConfig& config = {});

}   // namespace UIWidgets

}   // namespace Engine
