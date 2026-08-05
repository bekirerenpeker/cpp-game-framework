#include "graphics/ui/widgets/UIWidgets.hpp"
#include "graphics/ui/widgets/UIWidgetsInternal.hpp"
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

// Widget-specific proportions with no theme role of their own -- a track is not a
// control height and a dropdown is not a spacing step.
constexpr float TRACK_HEIGHT = 10.0f;
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
    const UIThemeColors& colors = UITheming::colors();
    const UIThemeMetrics& metrics = UITheming::metrics();

    ButtonConfig defaultConfig = {
        .buttonLayout =
            {
                           .padding = UIEdges(metrics.spacing.lg, metrics.spacing.sm),
                           .alignMain = UIAlign::Center,
                           .alignCross = UIAlign::Center,
                           },
        .buttonStyle =
            {
                           .backgroundColor = colors.accent,
                           .borderColor = colors.accentHover,
                           .borderWidth = metrics.borderWidth.thin,
                           .borderRadius = metrics.radius.md,
                           .onHover =
                    {
                        .backgroundColor = colors.accentHover,
                        .borderColor = colors.foreground,
                        .shadowColor = colors.accent,
                        .shadowBlurRadius = metrics.spacing.lg,
                    }, .onHeld =
                    {
                        .backgroundColor = colors.accentActive,
                        .borderColor = colors.accentHover,
                        .borderWidth = metrics.borderWidth.thick,
                        .shadowBlurRadius = metrics.spacing.sm,
                    }, .onPressed = {.backgroundColor = colors.onAccent},
                           },
        .textLayout = {},
        .textConfig = {.style = UITheming::textStyle("button")},
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

    const UIThemeColors& colors = UITheming::colors();
    const UIThemeMetrics& metrics = UITheming::metrics();

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
                         .padding = UIEdges(metrics.spacing.md, metrics.spacing.xs),
                         .alignMain = UIAlign::Center,
                         .alignCross = UIAlign::Center,
                         },
        .itemStyle =
            {
                         .onHover = {.backgroundColor = colors.surfaceHover},
                         .onHeld = {.backgroundColor = colors.accent},
                         },
        .selectedItemStyle =
            {
                         .backgroundColor = colors.accentMuted,
                         .onHover = {.backgroundColor = colors.accent},
                         },
        .itemTextLayout = {},
        .itemTextConfig = {.style = UITheming::textStyle("label")},
        .hoveredItemTextStyle = {.color = colors.foreground},
        .selectedItemTextStyle = {.color = colors.foreground},
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
    const UIThemeColors& colors = UITheming::colors();
    const UIThemeMetrics& metrics = UITheming::metrics();

    SliderConfig defaultConfig = {
        .sliderLayout =
            {
                           .width = UISizeSpec::grow(),
                           .gap = metrics.spacing.lg,
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
                           .backgroundColor = colors.surfaceSunken,
                           .borderColor = colors.border,
                           .borderWidth = metrics.borderWidth.thin,
                           .borderRadius = TRACK_HEIGHT * 0.5f,
                           .onHover = {.borderColor = colors.borderFocus},
                           },
        .fillStyle =
            {
                           .backgroundColor = colors.accent,
                           .borderRadius = TRACK_HEIGHT * 0.5f,
                           },
        .handleLayout =
            {
                           .width = UISizeSpec::fixed(metrics.iconSize),
                           .height = UISizeSpec::fixed(metrics.iconSize),
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
                           .backgroundColor = colors.foreground,
                           .borderColor = colors.accent,
                           .borderWidth = metrics.borderWidth.thick,
                           .borderRadius = metrics.iconSize * 0.5f,
                           .onHover = {.borderWidth = metrics.borderWidth.thick + 1.0f},
                           .onHeld = {.shadowColor = colors.accent, .shadowBlurRadius = metrics.spacing.lg},
                           },
        .labelLayout = {},
        .labelTextConfig = {.style = UITheming::textStyle("label")},
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
    const UIThemeColors& colors = UITheming::colors();
    const UIThemeMetrics& metrics = UITheming::metrics();

    CheckBoxConfig defaultConfig = {
        .rowLayout =
            {
                        .gap = metrics.spacing.sm,
                        .alignCross = UIAlign::Center,
                        },
        .rowStyle = {},
        .boxLayout =
            {
                        .width = UISizeSpec::fixed(metrics.controlHeightSmall - 2.0f),
                        .height = UISizeSpec::fixed(metrics.controlHeightSmall - 2.0f),
                        .alignMain = UIAlign::Center,
                        .alignCross = UIAlign::Center,
                        },
        .boxStyle =
            {
                        .backgroundColor = colors.surfaceSunken,
                        .borderColor = colors.borderStrong,
                        .borderWidth = metrics.borderWidth.thin,
                        .borderRadius = metrics.radius.sm,
                        .onHover = {.borderColor = colors.borderFocus},
                        },
        .checkedBoxStyle =
            {
                        .backgroundColor = colors.accent,
                        .borderColor = colors.accentHover,
                        },
        .markLayout =
            {
                        .width = UISizeSpec::fixed(metrics.spacing.sm + 2.0f),
                        .height = UISizeSpec::fixed(metrics.spacing.sm + 2.0f),
                        },
        .markStyle = {.backgroundColor = colors.onAccent, .borderRadius = 2.0f},
        .labelLayout = {},
        .labelTextConfig = {.style = UITheming::textStyle("body")},
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

    const UIThemeColors& colors = UITheming::colors();
    const UIThemeMetrics& metrics = UITheming::metrics();

    RadioGroupConfig defaultConfig = {
        .groupLayout =
            {
                          .gap = metrics.spacing.sm,
                          .direction = UILayoutDirection::Column,
                          },
        .groupStyle = {},
        .rowLayout =
            {
                          .gap = metrics.spacing.sm,
                          .alignCross = UIAlign::Center,
                          },
        .rowStyle = {},
        .dotLayout =
            {
                          .width = UISizeSpec::fixed(metrics.iconSize),
                          .height = UISizeSpec::fixed(metrics.iconSize),
                          },
        .dotStyle =
            {
                          .backgroundColor = colors.surfaceSunken,
                          .borderColor = colors.borderStrong,
                          .borderWidth = metrics.borderWidth.thin,
                          .borderRadius = metrics.iconSize * 0.5f,
                          .onHover = {.borderColor = colors.borderFocus},
                          },
        .selectedDotStyle =
            {
                          .backgroundColor = colors.accent,
                          .borderColor = colors.onAccent,
                          .borderWidth = metrics.borderWidth.thick * 2.0f,
                          },
        .labelLayout = {},
        .labelTextConfig = {.style = UITheming::textStyle("label")},
        .selectedLabelTextStyle = {.color = colors.foreground},
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

    const UIThemeColors& colors = UITheming::colors();
    const UIThemeMetrics& metrics = UITheming::metrics();

    bool& open = g_dropdownOpen[&selected];

    DropdownConfig defaultConfig = {
        .wrapperLayout = {},
        .buttonLayout =
            {
                          .width = UISizeSpec::fixed(DROPDOWN_WIDTH),
                          .padding = UIEdges(metrics.spacing.md, metrics.spacing.sm),
                          .gap = metrics.spacing.sm,
                          .alignCross = UIAlign::Center,
                          .clipX = true,
                          },
        .buttonStyle =
            {
                          .backgroundColor = colors.surfaceSunken,
                          .borderColor = colors.borderStrong,
                          .borderWidth = metrics.borderWidth.thin,
                          .borderRadius = metrics.radius.sm,
                          .overflow = UIOverflow::Hidden,
                          .onHover = {.borderColor = colors.borderFocus},
                          .onHeld = {.backgroundColor = colors.surfaceHover},
                          },
        // A zero floor on the label, so a string longer than the button shrinks and gets
        // cut instead of pushing the arrow out of the box.
        .labelLayout =
            {
                          .width = UISizeSpec::grow(),
                          .clipX = true,
                          },
        .labelTextConfig = {.style = UITheming::textStyle("body")},
        .arrowLayout =
            {
                          .width = UISizeSpec::fixed(DROPDOWN_ARROW_SIZE),
                          .height = UISizeSpec::fixed(DROPDOWN_ARROW_SIZE),
                          },
        .arrowStyle =
            {
                          .backgroundColor = colors.foregroundMuted,
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
                          .backgroundColor = colors.surfaceOverlay,
                          .borderColor = colors.borderStrong,
                          .borderWidth = metrics.borderWidth.thin,
                          .borderRadius = metrics.radius.md,
                          .shadowColor = colors.shadow,
                          .shadowOffset = Vec2(0.0f, metrics.spacing.sm),
                          .shadowBlurRadius = metrics.spacing.lg,
                          .overflow = UIOverflow::Hidden,
                          .ignoreClip = true,
                          .zIndex = 40,
                          },
        .itemLayout =
            {
                          .width = UISizeSpec::grow(),
                          .padding = UIEdges(metrics.spacing.sm, metrics.spacing.xs),
                          .alignCross = UIAlign::Center,
                          .clipX = true,
                          },
        .itemStyle =
            {
                          .borderRadius = metrics.radius.sm,
                          .overflow = UIOverflow::Hidden,
                          .onHover = {.backgroundColor = colors.surfaceHover},
                          .onHeld = {.backgroundColor = colors.accent},
                          },
        .selectedItemStyle = {.backgroundColor = colors.accentMuted},
        .itemTextLayout =
            {
                          .width = UISizeSpec::grow(),
                          .clipX = true,
                          },
        .itemTextConfig = {.style = UITheming::textStyle("body")},
        .selectedItemTextStyle = {.color = colors.foreground},
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
        // button still drops the list flush under itself -- unscaled, since that height
        // is in real pixels while a floating offset is in design units.
        defaultConfig.panelLayout.floating =
            UIFloatingConfig {.offset = Vec2(0.0f, unscale(button.size.y)) + config.panelOffset};

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
