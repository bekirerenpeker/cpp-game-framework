#include "graphics/ui/UIWidgets.hpp"
#include "UIWidgetsInternal.hpp"
#include "core/input/Input.hpp"
#include "graphics/ui/UiManager.hpp"
#include "utils/math/MathFuncs.hpp"
#include <format>
#include <string>
#include <unordered_map>
#include <vector>

namespace Engine {

namespace UIWidgets {

namespace {

constexpr float MENU_HEIGHT = 28.0f;
constexpr float TRACK_HEIGHT = 10.0f;
constexpr float HANDLE_SIZE = 16.0f;
constexpr float DROPDOWN_WIDTH = 170.0f;
constexpr float DROPDOWN_ARROW_SIZE = 12.0f;

float snapValue(float value, float minValue, float maxValue, float step)
{
    if (step > 0.0f) value = minValue + Math::round((value - minValue) / step) * step;
    return Math::clamp(value, minValue, maxValue);
}

// Keyed on the caller's own variable, the way colorPickerPopup keys on its colour: a
// dropdown is rebuilt from nothing every frame, so whether it is open can only live
// somewhere that outlives the frame, and the bound selection is the one thing the
// caller guarantees is stable.
std::unordered_map<const void*, bool> g_dropdownOpen;

}   // namespace

UINodeState button(const std::string text, const ButtonConfig& config)
{
    ButtonConfig defaultConfig = {
        .buttonLayout =
            {
                           .padding = UIEdges(18.0f, 10.0f),
                           .alignMain = UIAlign::Center,
                           .alignCross = UIAlign::Center,
                           },
        .buttonStyle =
            {
                           .backgroundColor = Color(0.24f, 0.52f, 0.92f),
                           .borderColor = Color(0.44f, 0.67f, 0.98f),
                           .borderWidth = 1.0f,
                           .borderRadius = 7.0f,
                           .onHover =
                    {
                        .backgroundColor = Color(0.33f, 0.61f, 0.98f),
                        .borderColor = COLOR_WHITE,
                        .shadowColor = Color(0.24f, 0.52f, 0.92f, 0.55f),
                        .shadowBlurRadius = 16.0f,
                    }, .onHeld =
                    {
                        .backgroundColor = Color(0.16f, 0.38f, 0.72f),
                        .borderColor = Color(0.62f, 0.78f, 1.0f),
                        .borderWidth = 2.0f,
                        .shadowBlurRadius = 6.0f,
                    }, .onPressed = {.backgroundColor = COLOR_WHITE},
                           },
        .textLayout = {},
        .textConfig = {.style = {.color = COLOR_WHITE, .size = 15.0f}},
    };

    defaultConfig.buttonLayout.combine(config.buttonLayout);
    defaultConfig.buttonStyle.combine(config.buttonStyle);
    defaultConfig.textLayout.combine(config.textLayout);
    defaultConfig.textConfig.style.combine(config.textConfig.style);

    UITextConfig textConfig = config.textConfig;
    textConfig.text = text;
    textConfig.style = defaultConfig.textConfig.style;

    UINodeState state =
        openContainer(defaultConfig.buttonLayout, defaultConfig.buttonStyle, config.key);
    addTextLeaf(defaultConfig.textLayout, textConfig);
    closeContainer();

    return state;
}
void toolbarMenu(
    const std::vector<std::string>& items, int& selected, const ToolbarMenuConfig& config
)
{
    int count = (int)items.size();
    if (count == 0) return;
    if (selected < 0 || selected >= count) selected = 0;

    ToolbarMenuConfig defaultConfig = {
        .menuLayout =
            {
                         .width = UISizeSpec::grow(),
                         .height = UISizeSpec::fit(),
                         },
        .menuStyle = {},
        .itemLayout =
            {
                         .width = UISizeSpec::grow(),
                         .height = UISizeSpec::grow(),
                         .padding = UIEdges(10.0f, 5.0f),
                         .alignMain = UIAlign::Center,
                         .alignCross = UIAlign::Center,
                         },
        .itemStyle =
            {
                         .onHover = {.backgroundColor = Color(0.26f, 0.29f, 0.37f)},
                         .onHeld = {.backgroundColor = Color(0.30f, 0.62f, 0.95f)},
                         },
        .selectedItemStyle =
            {
                         .backgroundColor = Color(0.24f, 0.38f, 0.58f),
                         .onHover = {.backgroundColor = Color(0.28f, 0.45f, 0.68f)},
                         },
        .itemTextLayout = {},
        .itemTextConfig = {.style = {.color = Color(0.72f, 0.75f, 0.82f), .size = 13.0f}},
        .hoveredItemTextStyle = {.color = Color(0.92f, 0.94f, 0.98f)},
        .selectedItemTextStyle = {.color = COLOR_WHITE},
    };

    defaultConfig.menuLayout.combine(config.menuLayout);
    defaultConfig.menuStyle.combine(config.menuStyle);
    defaultConfig.itemLayout.combine(config.itemLayout);
    defaultConfig.itemStyle.combine(config.itemStyle);
    defaultConfig.selectedItemStyle.combine(config.selectedItemStyle);
    defaultConfig.itemTextLayout.combine(config.itemTextLayout);
    defaultConfig.itemTextConfig.style.combine(config.itemTextConfig.style);
    defaultConfig.hoveredItemTextStyle.combine(config.hoveredItemTextStyle);
    defaultConfig.selectedItemTextStyle.combine(config.selectedItemTextStyle);

    openContainer(
        {.width = UISizeSpec::grow(), .direction = UILayoutDirection::Column}, {}, config.key
    );
    openContainer(defaultConfig.menuLayout, defaultConfig.menuStyle);

    // Snapshotted, so the press below cannot restyle half the bar against the other
    // half mid-loop; the new selection shows next frame like every other state here.
    int active = selected;
    for (int i = 0; i < count; i++) {
        UIContainerStyleSpec itemStyle = defaultConfig.itemStyle;
        if (i == active) itemStyle.combine(defaultConfig.selectedItemStyle);

        UINodeState item = openContainer(defaultConfig.itemLayout, itemStyle);
        if (item.isPressed) selected = i;

        UITextConfig itemText = config.itemTextConfig;
        itemText.text = items[i];
        itemText.style = defaultConfig.itemTextConfig.style;
        if (item.isHovered) itemText.style.combine(defaultConfig.hoveredItemTextStyle);
        if (i == active) itemText.style.combine(defaultConfig.selectedItemTextStyle);

        addTextLeaf(defaultConfig.itemTextLayout, itemText);
        closeContainer();

        if (i != count - 1) verticalDivider();
    }

    closeContainer();
    horizontalDivider();
    closeContainer();
}
void sliderFloat(
    const std::string label, float& value, float minValue, float maxValue,
    const SliderConfig& config
)
{
    SliderConfig defaultConfig = {
        .sliderLayout =
            {
                           .width = UISizeSpec::grow(),
                           .gap = 16.0f,
                           .direction = UILayoutDirection::Row,
                           .alignCross = UIAlign::Center,
                           },
        .sliderStyle = {},
        .trackLayout =
            {
                           .width = UISizeSpec::grow(),
                           .height = UISizeSpec::fixed(TRACK_HEIGHT),
                           .alignCross = UIAlign::Center,
                           },
        .trackStyle =
            {
                           .backgroundColor = Color(0.20f, 0.22f, 0.28f),
                           .borderColor = Color(0.28f, 0.31f, 0.40f),
                           .borderWidth = 1.0f,
                           .borderRadius = TRACK_HEIGHT * 0.5f,
                           .onHover = {.borderColor = Color(0.30f, 0.62f, 0.95f)},
                           },
        .fillStyle =
            {
                           .backgroundColor = Color(0.30f, 0.62f, 0.95f),
                           .borderRadius = TRACK_HEIGHT * 0.5f,
                           },
        .handleLayout =
            {
                           .width = UISizeSpec::fixed(HANDLE_SIZE),
                           .height = UISizeSpec::fixed(HANDLE_SIZE),
                           .floating =
                    UIFloatingConfig {
                        .anchorX = UIAlign::End,
                        .anchorY = UIAlign::Center,
                        .selfX = UIAlign::Center,
                        .selfY = UIAlign::Center
                    }, .isFloating = true,
                           },
        .handleStyle =
            {
                           .backgroundColor = COLOR_WHITE,
                           .borderColor = Color(0.30f, 0.62f, 0.95f),
                           .borderWidth = 2.0f,
                           .borderRadius = HANDLE_SIZE * 0.5f,
                           .onHover = {.borderWidth = 3.0f},
                           .onHeld = {.shadowColor = Color(0.30f, 0.62f, 0.95f), .shadowBlurRadius = 14.0f},
                           },
        .labelLayout = {},
        .labelTextConfig = {.style = {.color = Color(0.72f, 0.75f, 0.82f), .size = 12.0f}},
    };

    defaultConfig.sliderLayout.combine(config.sliderLayout);
    defaultConfig.sliderStyle.combine(config.sliderStyle);
    defaultConfig.trackLayout.combine(config.trackLayout);
    defaultConfig.trackStyle.combine(config.trackStyle);
    defaultConfig.fillStyle.combine(config.fillStyle);
    defaultConfig.handleLayout.combine(config.handleLayout);
    defaultConfig.handleStyle.combine(config.handleStyle);
    defaultConfig.labelLayout.combine(config.labelLayout);
    defaultConfig.labelTextConfig.style.combine(config.labelTextConfig.style);

    float range = maxValue - minValue;
    value = snapValue(value, minValue, maxValue, config.step);
    float fraction = range > 0.0f ? Math::clamp((value - minValue) / range, 0.0f, 1.0f) : 0.0f;

    openContainer(defaultConfig.sliderLayout, defaultConfig.sliderStyle, config.key);

    int decimals = config.decimals > 0 ? config.decimals : 0;
    UITextConfig labelText = config.labelTextConfig;
    std::string labelString = std::format("{}  {:.{}f}", label, value, decimals);
    labelText.text = labelString;
    labelText.style = defaultConfig.labelTextConfig.style;
    addTextLeaf(defaultConfig.labelLayout, labelText);

    UINodeState track = openContainer(defaultConfig.trackLayout, defaultConfig.trackStyle);

    // A Percent width keeps the fill honest without anyone knowing the track's pixel
    // width, and anchoring the handle to the fill's end rides on the same solve.
    UINodeState fill = openContainer(
        {.width = UISizeSpec::percent(fraction), .height = UISizeSpec::grow()},
        defaultConfig.fillStyle
    );
    UINodeState handle = openContainer(defaultConfig.handleLayout, defaultConfig.handleStyle);
    closeContainer();
    closeContainer();
    closeContainer();

    // The fill and the handle sit on top of the track, so a press lands on whichever of
    // the three is topmost; all of them drive the same value, read off the track's rect.
    // relativeMousePos is measured against that rect whether or not the cursor is still
    // inside it, which is what lets a drag run off the end and clamp.
    if (range > 0.0f && (track.isActive || fill.isActive || handle.isActive)) {
        value = minValue + Math::clamp(track.relativeMousePos.x, 0.0f, 1.0f) * range;
        value = snapValue(value, minValue, maxValue, config.step);
    }

    closeContainer();
}

void sliderInt(
    const std::string label, int& value, int minValue, int maxValueExcluded,
    const SliderConfig& config
)
{
    int maxValue = maxValueExcluded - 1 > minValue ? maxValueExcluded - 1 : minValue;

    SliderConfig intConfig = config;
    if (intConfig.step <= 0.0f) intConfig.step = 1.0f;
    intConfig.decimals = 0;

    float floatValue = (float)value;
    sliderFloat(label, floatValue, (float)minValue, (float)maxValue, intConfig);
    value = (int)Math::round(floatValue);
}

UINodeState checkBox(const std::string label, bool& checked, const CheckBoxConfig& config)
{
    CheckBoxConfig defaultConfig = {
        .rowLayout =
            {
                        .gap = 8.0f,
                        .alignCross = UIAlign::Center,
                        },
        .rowStyle = {},
        .boxLayout =
            {
                        .width = UISizeSpec::fixed(18.0f),
                        .height = UISizeSpec::fixed(18.0f),
                        .alignMain = UIAlign::Center,
                        .alignCross = UIAlign::Center,
                        },
        .boxStyle =
            {
                        .backgroundColor = Color(0.20f, 0.22f, 0.28f),
                        .borderColor = Color(0.38f, 0.42f, 0.52f),
                        .borderWidth = 1.0f,
                        .borderRadius = 4.0f,
                        .onHover = {.borderColor = Color(0.30f, 0.62f, 0.95f)},
                        },
        .checkedBoxStyle =
            {
                        .backgroundColor = Color(0.30f, 0.62f, 0.95f),
                        .borderColor = Color(0.62f, 0.80f, 1.0f),
                        },
        .markLayout =
            {
                        .width = UISizeSpec::fixed(8.0f),
                        .height = UISizeSpec::fixed(8.0f),
                        },
        .markStyle = {.backgroundColor = COLOR_WHITE, .borderRadius = 2.0f},
        .labelLayout = {},
        .labelTextConfig = {.style = {.color = Color(0.82f, 0.85f, 0.91f), .size = 13.0f}},
    };

    defaultConfig.rowLayout.combine(config.rowLayout);
    defaultConfig.rowStyle.combine(config.rowStyle);
    defaultConfig.boxLayout.combine(config.boxLayout);
    defaultConfig.boxStyle.combine(config.boxStyle);
    defaultConfig.checkedBoxStyle.combine(config.checkedBoxStyle);
    defaultConfig.markLayout.combine(config.markLayout);
    defaultConfig.markStyle.combine(config.markStyle);
    defaultConfig.labelLayout.combine(config.labelLayout);
    defaultConfig.labelTextConfig.style.combine(config.labelTextConfig.style);

    bool wasChecked = checked;

    UINodeState row = openContainer(defaultConfig.rowLayout, defaultConfig.rowStyle, config.key);
    if (row.isPressed) checked = !checked;

    UIContainerStyleSpec boxStyle = defaultConfig.boxStyle;
    if (wasChecked) boxStyle.combine(defaultConfig.checkedBoxStyle);

    openContainer(defaultConfig.boxLayout, boxStyle);
    if (wasChecked) {
        openContainer(defaultConfig.markLayout, defaultConfig.markStyle);
        closeContainer();
    }
    closeContainer();

    UITextConfig labelText = config.labelTextConfig;
    labelText.text = label;
    labelText.style = defaultConfig.labelTextConfig.style;
    addTextLeaf(defaultConfig.labelLayout, labelText);

    closeContainer();
    return row;
}

void radioGroup(
    const std::vector<std::string>& items, int& selected, const RadioGroupConfig& config
)
{
    int count = (int)items.size();
    if (count == 0) return;
    if (selected < 0 || selected >= count) selected = 0;

    RadioGroupConfig defaultConfig = {
        .groupLayout =
            {
                          .gap = 6.0f,
                          .direction = UILayoutDirection::Column,
                          },
        .groupStyle = {},
        .rowLayout =
            {
                          .gap = 8.0f,
                          .alignCross = UIAlign::Center,
                          },
        .rowStyle = {},
        .dotLayout =
            {
                          .width = UISizeSpec::fixed(16.0f),
                          .height = UISizeSpec::fixed(16.0f),
                          },
        .dotStyle =
            {
                          .backgroundColor = Color(0.20f, 0.22f, 0.28f),
                          .borderColor = Color(0.38f, 0.42f, 0.52f),
                          .borderWidth = 1.0f,
                          .borderRadius = 8.0f,
                          .onHover = {.borderColor = Color(0.30f, 0.62f, 0.95f)},
                          },
        .selectedDotStyle =
            {
                          .backgroundColor = Color(0.30f, 0.62f, 0.95f),
                          .borderColor = COLOR_WHITE,
                          .borderWidth = 4.0f,
                          },
        .labelLayout = {},
        .labelTextConfig = {.style = {.color = Color(0.72f, 0.75f, 0.82f), .size = 13.0f}},
        .selectedLabelTextStyle = {.color = COLOR_WHITE},
    };

    defaultConfig.groupLayout.combine(config.groupLayout);
    defaultConfig.groupStyle.combine(config.groupStyle);
    defaultConfig.rowLayout.combine(config.rowLayout);
    defaultConfig.rowStyle.combine(config.rowStyle);
    defaultConfig.dotLayout.combine(config.dotLayout);
    defaultConfig.dotStyle.combine(config.dotStyle);
    defaultConfig.selectedDotStyle.combine(config.selectedDotStyle);
    defaultConfig.labelLayout.combine(config.labelLayout);
    defaultConfig.labelTextConfig.style.combine(config.labelTextConfig.style);
    defaultConfig.selectedLabelTextStyle.combine(config.selectedLabelTextStyle);

    openContainer(defaultConfig.groupLayout, defaultConfig.groupStyle, config.key);

    int active = selected;
    for (int i = 0; i < count; i++) {
        UINodeState row = openContainer(defaultConfig.rowLayout, defaultConfig.rowStyle);
        if (row.isPressed) selected = i;

        UIContainerStyleSpec dotStyle = defaultConfig.dotStyle;
        if (i == active) dotStyle.combine(defaultConfig.selectedDotStyle);

        openContainer(defaultConfig.dotLayout, dotStyle);
        closeContainer();

        UITextConfig labelText = config.labelTextConfig;
        labelText.text = items[i];
        labelText.style = defaultConfig.labelTextConfig.style;
        if (i == active) labelText.style.combine(defaultConfig.selectedLabelTextStyle);
        addTextLeaf(defaultConfig.labelLayout, labelText);

        closeContainer();
    }

    closeContainer();
}

int dropdown(const std::vector<std::string>& items, int& selected, const DropdownConfig& config)
{
    int count = (int)items.size();
    if (count == 0) return 0;
    if (selected < 0 || selected >= count) selected = 0;

    bool& open = g_dropdownOpen[&selected];

    DropdownConfig defaultConfig = {
        .wrapperLayout = {},
        .buttonLayout =
            {
                          .width = UISizeSpec::fixed(DROPDOWN_WIDTH),
                          .padding = UIEdges(10.0f, 6.0f),
                          .gap = 8.0f,
                          .alignCross = UIAlign::Center,
                          .clipX = true,
                          },
        .buttonStyle =
            {
                          .backgroundColor = Color(0.20f, 0.22f, 0.28f),
                          .borderColor = Color(0.38f, 0.42f, 0.52f),
                          .borderWidth = 1.0f,
                          .borderRadius = 5.0f,
                          .overflow = UIOverflow::Hidden,
                          .onHover = {.borderColor = Color(0.30f, 0.62f, 0.95f)},
                          .onHeld = {.backgroundColor = Color(0.26f, 0.29f, 0.37f)},
                          },
        // A zero floor on the label, so a string longer than the button shrinks and gets
        // cut instead of pushing the arrow out of the box.
        .labelLayout =
            {
                          .width = UISizeSpec::grow(),
                          .clipX = true,
                          },
        .labelTextConfig = {.style = {.color = Color(0.86f, 0.89f, 0.94f), .size = 13.0f}},
        .arrowLayout =
            {
                          .width = UISizeSpec::fixed(DROPDOWN_ARROW_SIZE),
                          .height = UISizeSpec::fixed(DROPDOWN_ARROW_SIZE),
                          },
        .arrowStyle =
            {
                          .backgroundColor = Color(0.72f, 0.76f, 0.84f),
                          .backgroundImage = dropdownTexture(),
                          },
        .panelLayout =
            {
                          .width = UISizeSpec::percent(1.0f),
                          .padding = UIEdges(3.0f),
                          .direction = UILayoutDirection::Column,
                          .isFloating = true,
                          .clipX = true,
                          },
        .panelStyle =
            {
                          .backgroundColor = Color(0.14f, 0.15f, 0.20f),
                          .borderColor = Color(0.34f, 0.38f, 0.48f),
                          .borderWidth = 1.0f,
                          .borderRadius = 6.0f,
                          .shadowColor = Color(0.0f, 0.0f, 0.0f, 0.7f),
                          .shadowOffset = Vec2(0.0f, 6.0f),
                          .shadowBlurRadius = 18.0f,
                          .overflow = UIOverflow::Hidden,
                          .ignoreClip = true,
                          .zIndex = 40,
                          },
        .itemLayout =
            {
                          .width = UISizeSpec::grow(),
                          .padding = UIEdges(7.0f, 5.0f),
                          .alignCross = UIAlign::Center,
                          .clipX = true,
                          },
        .itemStyle =
            {
                          .borderRadius = 4.0f,
                          .overflow = UIOverflow::Hidden,
                          .onHover = {.backgroundColor = Color(0.26f, 0.29f, 0.37f)},
                          .onHeld = {.backgroundColor = Color(0.30f, 0.62f, 0.95f)},
                          },
        .selectedItemStyle = {.backgroundColor = Color(0.24f, 0.38f, 0.58f)},
        .itemTextLayout =
            {
                          .width = UISizeSpec::grow(),
                          .clipX = true,
                          },
        .itemTextConfig = {.style = {.color = Color(0.78f, 0.81f, 0.88f), .size = 13.0f}},
        .selectedItemTextStyle = {.color = COLOR_WHITE},
    };

    defaultConfig.wrapperLayout.combine(config.wrapperLayout);
    defaultConfig.buttonLayout.combine(config.buttonLayout);
    defaultConfig.buttonStyle.combine(config.buttonStyle);
    defaultConfig.labelLayout.combine(config.labelLayout);
    defaultConfig.labelTextConfig.style.combine(config.labelTextConfig.style);
    defaultConfig.arrowLayout.combine(config.arrowLayout);
    defaultConfig.arrowStyle.combine(config.arrowStyle);
    defaultConfig.panelLayout.combine(config.panelLayout);
    defaultConfig.panelStyle.combine(config.panelStyle);
    defaultConfig.itemLayout.combine(config.itemLayout);
    defaultConfig.itemStyle.combine(config.itemStyle);
    defaultConfig.selectedItemStyle.combine(config.selectedItemStyle);
    defaultConfig.itemTextLayout.combine(config.itemTextLayout);
    defaultConfig.itemTextConfig.style.combine(config.itemTextConfig.style);
    defaultConfig.selectedItemTextStyle.combine(config.selectedItemTextStyle);

    openContainer(defaultConfig.wrapperLayout, {}, config.key);

    UINodeState button = openContainer(defaultConfig.buttonLayout, defaultConfig.buttonStyle);

    // A caller-supplied label makes this a menu rather than a select: it keeps reading
    // the same whatever is selected, which is what a toolbar entry wants.
    UITextConfig labelText = config.labelTextConfig;
    labelText.text = config.label.empty() ? std::string_view(items[selected]) : config.label;
    labelText.style = defaultConfig.labelTextConfig.style;
    // Not left to the caller: one clipped line is what the widget is, and a wrapping
    // label would grow the closed dropdown to whatever the longest entry needs.
    labelText.wrapEnabled = false;
    labelText.overflow = TextOverflow::Ellipsis;
    addTextLeaf(defaultConfig.labelLayout, labelText);

    UIContainerStyleSpec arrowStyle = defaultConfig.arrowStyle;
    arrowStyle.imageRotation = open ? (float)PI : 0.0f;
    openContainer(defaultConfig.arrowLayout, arrowStyle);
    closeContainer();

    closeContainer();

    if (button.isPressed) open = !open;

    if (open) {
        // Off the button's solved height rather than the configured one, so a resized
        // button still drops the list flush under itself.
        defaultConfig.panelLayout.floating =
            UIFloatingConfig {.offset = Vec2(0.0f, button.size.y) + config.panelOffset};

        UINodeState panel = openContainer(defaultConfig.panelLayout, defaultConfig.panelStyle);

        for (int i = 0; i < count; i++) {
            UIContainerStyleSpec itemStyle = defaultConfig.itemStyle;
            if (i == selected) itemStyle.combine(defaultConfig.selectedItemStyle);

            UINodeState item = openContainer(defaultConfig.itemLayout, itemStyle);

            UITextConfig itemText = config.itemTextConfig;
            itemText.text = items[i];
            itemText.style = defaultConfig.itemTextConfig.style;
            if (i == selected) itemText.style.combine(defaultConfig.selectedItemTextStyle);
            itemText.wrapEnabled = false;
            itemText.overflow = TextOverflow::Ellipsis;
            addTextLeaf(defaultConfig.itemTextLayout, itemText);

            closeContainer();

            if (item.isPressed) {
                selected = i;
                open = false;
            }
        }

        closeContainer();

        if (Input::get().mouseButtonPressed(MouseButton::Left) && !panel.isHovered &&
            !button.isHovered) {
            open = false;
        }
    }

    closeContainer();
    return selected;
}

}   // namespace UIWidgets

}   // namespace Engine
