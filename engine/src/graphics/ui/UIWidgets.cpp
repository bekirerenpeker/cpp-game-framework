#include "graphics/ui/UIWidgets.hpp"
#include "core/file_management/FileManager.hpp"
#include "core/input/Input.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "graphics/gl_wrappers/GlShader.hpp"
#include "graphics/gl_wrappers/GlTexture.hpp"
#include "graphics/ui/UIRenderer.hpp"
#include "graphics/ui/UiManager.hpp"
#include "utils/math/MathFuncs.hpp"
#include <format>
#include <string>
#include <unordered_map>
#include <vector>

namespace Engine {

namespace UIWidgets {

namespace {

constexpr float TITLE_BAR_HEIGHT = 30.0f;
constexpr float GRIP_SIZE = 28.0f;
constexpr float DIVIDER_THICKNESS = 1.0f;
constexpr float MENU_HEIGHT = 28.0f;
constexpr float TRACK_HEIGHT = 10.0f;
constexpr float HANDLE_SIZE = 16.0f;
const Color DIVIDER_COLOR(0.28f, 0.31f, 0.40f);
const Vec2 WINDOW_MIN(200.0f, 120.0f);
const Vec2 POS_UNBOUNDED(-UI_UNBOUNDED, -UI_UNBOUNDED);

struct UIDragState
{
    bool isActive = false;
    Vec2 pointerOrigin = VEC2_ZERO;
    Vec2 valueOrigin = VEC2_ZERO;
};

struct UIWindowState
{
    Vec2 pos = Vec2(60.0f, 60.0f);
    Vec2 size = Vec2(700.0f, 500.0f);
    bool isCollapsed = false;

    UIDragState moveDrag;
    UIDragState resizeDrag;
};

// Keyed rather than positional, and never erased, because the tree is rebuilt every
// frame and this is the only thing about a window that outlives it.
std::unordered_map<std::string, UIWindowState> g_windowStates;
std::vector<UIWindowState*> g_openWindows;

// Deliberately leaked: a static GlTexture would run its destructor after the GL context
// is already gone, and a widget's own asset has no owner to outlive it.
GlTexture* dragHandleTexture()
{
    static GlTexture* texture = nullptr;
    static bool loaded = false;
    if (loaded) return texture;

    loaded = true;
    fs::path path = FileManager::get().engineAsset("images/resize-handle.png");
    if (!FileManager::get().doesPathExist(path)) {
        LOG_ERROR("drag handle image missing at {}", path);
        return texture;
    }

    texture =
        new GlTexture(path, 0, GlTexture::FilterMode::Linear, GlTexture::WrapMode::ClampToEdge);
    return texture;
}

GlShader* colorPickerShader()
{
    static GlShader* shader = nullptr;
    static bool loaded = false;
    if (loaded) return shader;

    loaded = true;
    fs::path path = FileManager::get().engineAsset("shaders/UIColorPickerShader.glsl");
    if (!FileManager::get().doesPathExist(path)) {
        LOG_ERROR("colour picker shader missing at {}", path);
        return shader;
    }

    shader = new GlShader(path);
    return shader;
}

// Hue and saturation have no meaning in a black or greyscale RGB value, so they are
// kept here per picker instead of being re-derived every frame and lost.
std::unordered_map<const Color*, Vec3> g_pickerHsv;
std::unordered_map<const Color*, bool> g_pickerOpen;

float snapValue(float value, float minValue, float maxValue, float step)
{
    if (step > 0.0f) value = minValue + Math::round((value - minValue) / step) * step;
    return Math::clamp(value, minValue, maxValue);
}

void dragVec2(const UINodeState& handle, UIDragState& drag, Vec2& value, Vec2 minValue)
{
    if (!handle.isActive) {
        drag.isActive = false;
        return;
    }

    Vec2 mouse = UIRenderer::get().getMouseUiPos();
    if (!drag.isActive) {
        drag.isActive = true;
        drag.pointerOrigin = mouse;
        drag.valueOrigin = value;
    }

    // The mouse is y-up while both the position and the size are y-down.
    Vec2 delta = mouse - drag.pointerOrigin;
    value = Vec2(
        Math::max(drag.valueOrigin.x + delta.x, minValue.x),
        Math::max(drag.valueOrigin.y - delta.y, minValue.y)
    );
}

}   // namespace

// base components interfaces
void clear() { UIManager::get().clear(); }
UINode* getNode(IdType id) { return UIManager::get().getNode(id); }

UINodeState
openContainer(const UILayoutConfig& layout, const UIContainerStyleSpec& style, std::string_view key)
{
    return UIManager::get().openContainer(layout, style, key);
}
void closeContainer() { UIManager::get().closeContainer(); }

IdType addTextLeaf(const UILayoutConfig& layout, const UITextConfig& config)
{
    return UIManager::get().addTextLeaf(layout, config);
}

IdType addShaderLeaf(const UILayoutConfig& layout, const UIShaderConfig& config)
{
    return UIManager::get().addShaderLeaf(layout, config);
}

// composite widgets
UINodeState dragHandle(const DragHandleConfig& config)
{
    DragHandleConfig defaultConfig = {
        .handleLayout =
            {
                           .width = UISizeSpec::fixed(GRIP_SIZE),
                           .height = UISizeSpec::fixed(GRIP_SIZE),
                           .floating =
                    UIFloatingConfig {
                        .offset = Vec2(-4.0f, -4.0f),
                        .anchorX = UIAlign::End,
                        .anchorY = UIAlign::End,
                        .selfX = UIAlign::End,
                        .selfY = UIAlign::End
                    }, .isFloating = true,
                           },
        // The image is white, so backgroundColor is what tints it.
        .handleStyle = {
                           .backgroundColor = Color(0.45f, 0.49f, 0.60f),
                           .backgroundImage = dragHandleTexture(),
                           .onHover = {.backgroundColor = COLOR_WHITE},
                           .onHeld = {.backgroundColor = Color(0.30f, 0.62f, 0.95f)},
                           },
    };

    defaultConfig.handleLayout.combine(config.handleLayout);
    defaultConfig.handleStyle.combine(config.handleStyle);

    UINodeState state =
        openContainer(defaultConfig.handleLayout, defaultConfig.handleStyle, config.key);
    closeContainer();

    return state;
}

UINodeState openWindow(const std::string& name, const WindowConfig& config)
{
    std::string_view windowKey = config.key.empty() ? std::string_view(name) : config.key;
    UIWindowState& state = g_windowStates[std::string(windowKey)];

    WindowConfig defaultConfig = {
        .windowLayout =
            {
                           .width = UISizeSpec::fixed(state.size.x),
                           .height = UISizeSpec::fixed(state.size.y),
                           .floating = UIFloatingConfig {.offset = state.pos},
                           .direction = UILayoutDirection::Column,
                           .isFloating = true,
                           },
        .windowStyle =
            {
                           .backgroundColor = Color(0.16f, 0.17f, 0.22f),
                           .borderColor = Color(0.28f, 0.31f, 0.40f),
                           .borderWidth = 1.0f,
                           .borderRadius = 10.0f,
                           .shadowColor = Color(0.0f, 0.0f, 0.0f, 0.75f),
                           .shadowOffset = Vec2(0.0f, 10.0f),
                           .shadowBlurRadius = 26.0f,
                           .overflow = UIOverflow::Hidden,
                           },
        .titleLayout =
            {
                           .width = UISizeSpec::grow(),
                           .height = UISizeSpec::fixed(TITLE_BAR_HEIGHT),
                           .padding = UIEdges(12.0f, 0.0f),
                           .alignCross = UIAlign::Center,
                           },
        .titleStyle =
            {
                           .backgroundColor = Color(0.24f, 0.26f, 0.34f),
                           // Rounded into the window's own top corners, square where it
                // meets the body.
                .borderRadius = UICorners(9.0f, 9.0f, 0.0f, 0.0f),
                           .onHover = {.backgroundColor = Color(0.30f, 0.33f, 0.42f)},
                           .onHeld = {.backgroundColor = Color(0.30f, 0.62f, 0.95f)},
                           },
        .titleTextConfig = {.style = {.color = Color(0.90f, 0.92f, 0.96f), .size = 14.0f}},
        .bodyLayout =
            {
                           .width = UISizeSpec::grow(),
                           .height = UISizeSpec::grow(),
                           .padding = UIEdges(12.0f),
                           .gap = 8.0f,
                           .direction = UILayoutDirection::Column,
                           },
        .bodyStyle = {},
    };

    defaultConfig.windowLayout.combine(config.windowLayout);
    defaultConfig.windowStyle.combine(config.windowStyle);
    defaultConfig.titleLayout.combine(config.titleLayout);
    defaultConfig.titleStyle.combine(config.titleStyle);
    defaultConfig.titleTextConfig.style.combine(config.titleTextConfig.style);
    defaultConfig.bodyLayout.combine(config.bodyLayout);
    defaultConfig.bodyStyle.combine(config.bodyStyle);

    // A root's own position is forced to zero and floating is applied by the parent, so
    // a top-level window needs this invisible wrapper to have something to float in.
    // Floating too, so a nested window books no slot in the list around it.
    openContainer({.isFloating = true}, {}, windowKey);

    UINodeState window =
        openContainer(defaultConfig.windowLayout, defaultConfig.windowStyle, windowKey);

    UINodeState titleBar = openContainer(defaultConfig.titleLayout, defaultConfig.titleStyle);
    UITextConfig titleTextConfig = config.titleTextConfig;
    titleTextConfig.text = name;
    titleTextConfig.style = defaultConfig.titleTextConfig.style;
    addTextLeaf({}, titleTextConfig);
    closeContainer();

    dragVec2(titleBar, state.moveDrag, state.pos, POS_UNBOUNDED);

    openContainer(defaultConfig.bodyLayout, defaultConfig.bodyStyle);

    g_openWindows.push_back(&state);
    return window;
}

void closeWindow()
{
    if (g_openWindows.empty()) {
        LOG_ERROR("closeWindow called with no window open; the open/close pairs are unbalanced");
        return;
    }

    UIWindowState& state = *g_openWindows.back();
    g_openWindows.pop_back();

    closeContainer();

    UINodeState grip = dragHandle();
    dragVec2(grip, state.resizeDrag, state.size, WINDOW_MIN);

    closeContainer();
    closeContainer();
}

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

UINodeState horizontalDivider(const DividerConfig& config)
{
    DividerConfig defaultConfig = {
        .dividerLayout =
            {
                            .width = UISizeSpec::grow(),
                            .height = UISizeSpec::fixed(DIVIDER_THICKNESS),
                            },
        .dividerStyle = {.backgroundColor = DIVIDER_COLOR                   },
    };

    defaultConfig.dividerLayout.combine(config.dividerLayout);
    defaultConfig.dividerStyle.combine(config.dividerStyle);

    UINodeState state =
        openContainer(defaultConfig.dividerLayout, defaultConfig.dividerStyle, config.key);
    closeContainer();

    return state;
}

UINodeState verticalDivider(const DividerConfig& config)
{
    DividerConfig defaultConfig = {
        .dividerLayout =
            {
                            .width = UISizeSpec::fixed(DIVIDER_THICKNESS),
                            .height = UISizeSpec::grow(),
                            },
        .dividerStyle = {             .backgroundColor = DIVIDER_COLOR },
    };

    defaultConfig.dividerLayout.combine(config.dividerLayout);
    defaultConfig.dividerStyle.combine(config.dividerStyle);

    UINodeState state =
        openContainer(defaultConfig.dividerLayout, defaultConfig.dividerStyle, config.key);
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

IdType text(const std::string label, const TextConfig& config)
{
    TextConfig defaultConfig = {
        .textLayout = {},
        .textConfig = {.style = {.color = Color(0.82f, 0.85f, 0.91f), .size = 14.0f}},
    };

    defaultConfig.textLayout.combine(config.textLayout);
    defaultConfig.textConfig.style.combine(config.textConfig.style);

    UITextConfig textConfig = config.textConfig;
    textConfig.text = label;
    textConfig.style = defaultConfig.textConfig.style;

    return addTextLeaf(defaultConfig.textLayout, textConfig);
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

void tooltip(const std::string label, bool visible, const TooltipConfig& config)
{
    // Declared whether or not the tooltip shows, because localMousePos comes from the
    // previous frame: an anchor created only on the frame the tooltip appears has no
    // geometry yet, and the tooltip flashes at the parent's corner before snapping to
    // the cursor. Keeping it alive also keeps its siblings' positional keys stable.
    // Input-transparent for the whole subtree, or a tooltip drawn over the widget that
    // spawned it takes the hover away from it, hides itself, and oscillates every frame.
    UINodeState anchor = openContainer({.isFloating = true}, {.ignoreInput = true}, config.key);
    if (!visible) {
        closeContainer();
        return;
    }

    TooltipConfig defaultConfig = {
        .tooltipLayout =
            {
                            .padding = UIEdges(8.0f, 5.0f),
                            .isFloating = true,
                            },
        .tooltipStyle =
            {
                            .backgroundColor = Color(0.10f, 0.11f, 0.15f, 0.96f),
                            .borderColor = Color(0.34f, 0.38f, 0.48f),
                            .borderWidth = 1.0f,
                            .borderRadius = 5.0f,
                            .shadowColor = Color(0.0f, 0.0f, 0.0f, 0.6f),
                            .shadowOffset = Vec2(0.0f, 4.0f),
                            .shadowBlurRadius = 12.0f,
                            .ignoreClip = true,
                            .zIndex = 64,
                            },
        .labelLayout = {},
        .labelTextConfig = {.style = {.color = Color(0.90f, 0.92f, 0.96f), .size = 12.0f}},
    };

    defaultConfig.tooltipLayout.combine(config.tooltipLayout);
    defaultConfig.tooltipStyle.combine(config.tooltipStyle);
    defaultConfig.labelLayout.combine(config.labelLayout);
    defaultConfig.labelTextConfig.style.combine(config.labelTextConfig.style);

    // A floating offset is relative to the parent. localMousePos already is; an explicit
    // anchor is measured from the root, so it needs the anchor node's own position taken
    // off it -- both end up in the same top-left y-down space. The self alignment is
    // what centres the tooltip on the point rather than hanging it off the corner; the
    // caller cannot do that itself, since it would need the tooltip's solved size.
    if (config.anchor) {
        defaultConfig.tooltipLayout.floating = UIFloatingConfig {
            .offset = *config.anchor - anchor.pos + config.anchorOffset,
            .selfX = config.anchorAlignX,
            .selfY = config.anchorAlignY,
        };
    } else {
        defaultConfig.tooltipLayout.floating =
            UIFloatingConfig {.offset = anchor.localMousePos + config.cursorOffset};
    }

    openContainer(defaultConfig.tooltipLayout, defaultConfig.tooltipStyle);

    UITextConfig labelText = config.labelTextConfig;
    labelText.text = label;
    labelText.style = defaultConfig.labelTextConfig.style;
    addTextLeaf(defaultConfig.labelLayout, labelText);

    closeContainer();
    closeContainer();
}

void colorPicker(Color& color, const ColorPickerConfig& config)
{
    ColorPickerConfig defaultConfig = {
        .pickerLayout =
            {
                           .width = UISizeSpec::grow(),
                           .gap = 8.0f,
                           .direction = UILayoutDirection::Column,
                           },
        .pickerStyle = {},
        .squareLayout =
            {
                           .width = UISizeSpec::grow(),
                           .height = UISizeSpec::grow(),
                           },
        .hueLayout =
            {
                           .width = UISizeSpec::fixed(config.hueWidth > 0.0f ? config.hueWidth : 18.0f),
                           .height = UISizeSpec::grow(),
                           },
        .markerLayout =
            {
                           .width = UISizeSpec::fixed(12.0f),
                           .height = UISizeSpec::fixed(12.0f),
                           .floating =
                    UIFloatingConfig {
                        .anchorX = UIAlign::End,
                        .anchorY = UIAlign::End,
                        .selfX = UIAlign::Center,
                        .selfY = UIAlign::Center
                    }, .isFloating = true,
                           },
        .markerStyle =
            {
                           .borderColor = COLOR_WHITE,
                           .borderWidth = 2.0f,
                           .borderRadius = 6.0f,
                           .shadowColor = Color(0.0f, 0.0f, 0.0f, 0.8f),
                           .shadowBlurRadius = 3.0f,
                           },
        .hueMarkerLayout =
            {
                           .width = UISizeSpec::percent(1.0f),
                           .height = UISizeSpec::fixed(4.0f),
                           .floating = UIFloatingConfig {.anchorY = UIAlign::End, .selfY = UIAlign::Center},
                           .isFloating = true,
                           },
        .hueMarkerStyle =
            {
                           .backgroundColor = COLOR_WHITE,
                           .borderColor = Color(0.0f, 0.0f, 0.0f, 0.6f),
                           .borderWidth = 1.0f,
                           .borderRadius = 2.0f,
                           },
        .alphaLayout =
            {
                           .width = UISizeSpec::grow(),
                           .height = UISizeSpec::fixed(config.alphaHeight > 0.0f ? config.alphaHeight : 14.0f),
                           },
        .alphaMarkerLayout =
            {
                           .width = UISizeSpec::fixed(4.0f),
                           .height = UISizeSpec::percent(1.0f),
                           .floating = UIFloatingConfig {.anchorX = UIAlign::End, .selfX = UIAlign::Center},
                           .isFloating = true,
                           },
        .alphaMarkerStyle =
            {
                           .backgroundColor = COLOR_WHITE,
                           .borderColor = Color(0.0f, 0.0f, 0.0f, 0.6f),
                           .borderWidth = 1.0f,
                           .borderRadius = 2.0f,
                           },
        .previewLayout =
            {
                           .width = UISizeSpec::grow(),
                           .height = UISizeSpec::fixed(22.0f),
                           },
        .previewStyle =
            {
                           .borderColor = Color(0.38f, 0.42f, 0.52f),
                           .borderWidth = 1.0f,
                           .borderRadius = 4.0f,
                           },
        .valuesLayout =
            {
                           .width = UISizeSpec::grow(),
                           .gap = 2.0f,
                           .direction = UILayoutDirection::Column,
                           },
        .valuesTextConfig = {.style = {.color = Color(0.66f, 0.70f, 0.78f), .size = 11.0f}},
    };

    defaultConfig.pickerLayout.combine(config.pickerLayout);
    defaultConfig.pickerStyle.combine(config.pickerStyle);
    defaultConfig.squareLayout.combine(config.squareLayout);
    defaultConfig.hueLayout.combine(config.hueLayout);
    defaultConfig.markerLayout.combine(config.markerLayout);
    defaultConfig.markerStyle.combine(config.markerStyle);
    defaultConfig.hueMarkerLayout.combine(config.hueMarkerLayout);
    defaultConfig.hueMarkerStyle.combine(config.hueMarkerStyle);
    defaultConfig.alphaLayout.combine(config.alphaLayout);
    defaultConfig.alphaMarkerLayout.combine(config.alphaMarkerLayout);
    defaultConfig.alphaMarkerStyle.combine(config.alphaMarkerStyle);
    defaultConfig.previewLayout.combine(config.previewLayout);
    defaultConfig.previewStyle.combine(config.previewStyle);
    defaultConfig.valuesLayout.combine(config.valuesLayout);
    defaultConfig.valuesTextConfig.style.combine(config.valuesTextConfig.style);

    Vec3& hsv = g_pickerHsv.try_emplace(&color, color.toHsv()).first->second;
    // The caller may have written the colour itself since last frame; only then is the
    // stored hue thrown away, so dragging to black does not lose it.
    Color fromState = Color::fromHsv(hsv, color.a);
    if (Math::abs(fromState.r - color.r) > 0.001f || Math::abs(fromState.g - color.g) > 0.001f ||
        Math::abs(fromState.b - color.b) > 0.001f) {
        hsv = color.toHsv();
    }

    float squareHeight = config.squareHeight > 0.0f ? config.squareHeight : 120.0f;

    openContainer(defaultConfig.pickerLayout, defaultConfig.pickerStyle, config.key);

    openContainer({
        .width = UISizeSpec::grow(),
        .height = UISizeSpec::fixed(squareHeight),
        .gap = 8.0f,
    });

    // Neither marker can be placed in pixels, since the square's size is only known once
    // the solve runs. A floating Percent-sized spacer reaches exactly the fraction, and
    // the marker anchors to its far corner -- the same trick the slider handle uses.
    // A zero intrinsic size, because these fill whatever their container is: the default
    // reports itself as min-content too, and nothing can be shrunk below that -- the leaf
    // would keep its own width and paint straight out of a narrower parent.
    UINodeState square = openContainer(defaultConfig.squareLayout);
    addShaderLeaf(
        {.width = UISizeSpec::grow(), .height = UISizeSpec::grow()},
        {.shader = colorPickerShader(),
         .params0 = Vec4(0.0f, hsv.x, 0.0f, 1.0f),
         .intrinsicSize = VEC2_ZERO}
    );
    UINodeState squareAnchor = openContainer({
        .width = UISizeSpec::percent(Math::clamp(hsv.y, 0.0f, 1.0f)),
        .height = UISizeSpec::percent(Math::clamp(1.0f - hsv.z, 0.0f, 1.0f)),
        .isFloating = true,
    });
    UINodeState squareMarker = openContainer(defaultConfig.markerLayout, defaultConfig.markerStyle);
    closeContainer();
    closeContainer();
    closeContainer();

    UINodeState hueStrip = openContainer(defaultConfig.hueLayout);
    addShaderLeaf(
        {.width = UISizeSpec::grow(), .height = UISizeSpec::grow()},
        {.shader = colorPickerShader(),
         .params0 = Vec4(1.0f, 0.0f, 0.0f, 1.0f),
         .intrinsicSize = VEC2_ZERO}
    );
    UINodeState hueAnchor = openContainer({
        .width = UISizeSpec::percent(1.0f),
        .height = UISizeSpec::percent(Math::clamp(hsv.x, 0.0f, 1.0f)),
        .isFloating = true,
    });
    UINodeState hueMarker =
        openContainer(defaultConfig.hueMarkerLayout, defaultConfig.hueMarkerStyle);
    closeContainer();
    closeContainer();
    closeContainer();

    closeContainer();

    float alpha = Math::clamp(color.a, 0.0f, 1.0f);
    Color opaque = Color::fromHsv(hsv, 1.0f);

    UINodeState alphaStrip = openContainer(defaultConfig.alphaLayout);
    addShaderLeaf(
        {.width = UISizeSpec::grow(), .height = UISizeSpec::grow()},
        {.shader = colorPickerShader(),
         .params0 = Vec4(2.0f, 0.0f, 0.0f, 1.0f),
         .params1 = Vec4(opaque.r, opaque.g, opaque.b, 1.0f),
         .intrinsicSize = VEC2_ZERO}
    );
    UINodeState alphaAnchor = openContainer({
        .width = UISizeSpec::percent(alpha),
        .height = UISizeSpec::percent(1.0f),
        .isFloating = true,
    });
    UINodeState alphaMarker =
        openContainer(defaultConfig.alphaMarkerLayout, defaultConfig.alphaMarkerStyle);
    closeContainer();
    closeContainer();
    closeContainer();

    // The markers and their spacers sit over the shader quads, so a press can land on
    // any of them; all of them drive the value off their own control's rect.
    if (square.isActive || squareAnchor.isActive || squareMarker.isActive) {
        hsv.y = Math::clamp(square.relativeMousePos.x, 0.0f, 1.0f);
        hsv.z = 1.0f - Math::clamp(square.relativeMousePos.y, 0.0f, 1.0f);
    }
    if (hueStrip.isActive || hueAnchor.isActive || hueMarker.isActive)
        hsv.x = Math::clamp(hueStrip.relativeMousePos.y, 0.0f, 0.9999f);
    if (alphaStrip.isActive || alphaAnchor.isActive || alphaMarker.isActive)
        alpha = Math::clamp(alphaStrip.relativeMousePos.x, 0.0f, 1.0f);

    color = Color::fromHsv(hsv, alpha);

    UIContainerStyleSpec previewStyle = defaultConfig.previewStyle;
    previewStyle.backgroundColor = color;
    openContainer(defaultConfig.previewLayout, previewStyle);
    closeContainer();

    if (config.showValues) {
        openContainer(defaultConfig.valuesLayout);

        UITextConfig valueText = config.valuesTextConfig;
        valueText.style = defaultConfig.valuesTextConfig.style;

        std::string rgba =
            std::format("RGBA  {:.2f}  {:.2f}  {:.2f}  {:.2f}", color.r, color.g, color.b, color.a);
        valueText.text = rgba;
        addTextLeaf({}, valueText);

        std::string hsva =
            std::format("HSVA  {:.2f}  {:.2f}  {:.2f}  {:.2f}", hsv.x, hsv.y, hsv.z, color.a);
        valueText.text = hsva;
        addTextLeaf({}, valueText);

        closeContainer();
    }

    closeContainer();
}

void colorPickerPopup(Color& color, const ColorPickerPopupConfig& config)
{
    ColorPickerPopupConfig defaultConfig = {
        .wrapperLayout = {},
        .swatchLayout =
            {
                          .width = UISizeSpec::fixed(46.0f),
                          .height = UISizeSpec::fixed(22.0f),
                          },
        .swatchStyle =
            {
                          .borderColor = Color(0.38f, 0.42f, 0.52f),
                          .borderWidth = 1.0f,
                          .borderRadius = 4.0f,
                          .onHover = {.borderColor = COLOR_WHITE},
                          },
        .panelLayout =
            {
                          .width = UISizeSpec::fixed(220.0f),
                          .padding = UIEdges(10.0f),
                          .isFloating = true,
                          },
        .panelStyle = {
                          .backgroundColor = Color(0.14f, 0.15f, 0.20f),
                          .borderColor = Color(0.34f, 0.38f, 0.48f),
                          .borderWidth = 1.0f,
                          .borderRadius = 8.0f,
                          .shadowColor = Color(0.0f, 0.0f, 0.0f, 0.7f),
                          .shadowOffset = Vec2(0.0f, 6.0f),
                          .shadowBlurRadius = 20.0f,
                          .ignoreClip = true,
                          .zIndex = 32,
                          },
    };

    defaultConfig.wrapperLayout.combine(config.wrapperLayout);
    defaultConfig.swatchLayout.combine(config.swatchLayout);
    defaultConfig.swatchStyle.combine(config.swatchStyle);
    defaultConfig.panelLayout.combine(config.panelLayout);
    defaultConfig.panelStyle.combine(config.panelStyle);

    bool& open = g_pickerOpen[&color];

    openContainer(defaultConfig.wrapperLayout, {}, config.key);

    UIContainerStyleSpec swatchStyle = defaultConfig.swatchStyle;
    swatchStyle.backgroundColor = color;
    UINodeState swatch = openContainer(defaultConfig.swatchLayout, swatchStyle);
    closeContainer();

    if (swatch.isPressed) open = !open;

    if (open) {
        // Anchored off the swatch's own solved height rather than the configured one, so
        // a caller-resized swatch still drops the panel flush under it.
        defaultConfig.panelLayout.floating =
            UIFloatingConfig {.offset = Vec2(0.0f, swatch.size.y) + config.panelOffset};

        UINodeState panel = openContainer(defaultConfig.panelLayout, defaultConfig.panelStyle);
        colorPicker(color, config.pickerConfig);
        closeContainer();

        if (Input::get().mouseButtonPressed(MouseButton::Left) && !panel.isHovered &&
            !swatch.isHovered) {
            open = false;
        }
    }

    closeContainer();
}

}   // namespace UIWidgets

}   // namespace Engine
