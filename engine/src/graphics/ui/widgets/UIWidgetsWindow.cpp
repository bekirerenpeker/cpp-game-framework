#include "graphics/ui/widgets/UIWidgets.hpp"
#include "graphics/ui/widgets/UIWidgetsInternal.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "graphics/ui/UIRenderer.hpp"
#include "graphics/ui/UiManager.hpp"
#include "utils/math/MathFuncs.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace Engine {

namespace UIWidgets {

namespace {

constexpr float TITLE_BAR_HEIGHT = 30.0f;
constexpr float GRIP_SIZE = 28.0f;
constexpr float COLLAPSE_BUTTON_SIZE = 20.0f;
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

// The body's id has to survive to closeWindow, which is where a collapsed window throws
// away whatever the caller declared into it.
struct UIOpenWindow
{
    UIWindowState* state = nullptr;
    IdType bodyId = INVALID_ID;
};

// Keyed rather than positional, and never erased, because the tree is rebuilt every
// frame and this is the only thing about a window that outlives it.
std::unordered_map<std::string, UIWindowState> g_windowStates;
std::vector<UIOpenWindow> g_openWindows;

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
    bool collapsed = state.isCollapsed;

    WindowConfig defaultConfig = {
        .windowLayout =
            {
                           .width = UISizeSpec::fixed(state.size.x),
                           .height = UISizeSpec::fixed(collapsed ? TITLE_BAR_HEIGHT : state.size.y),
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
                // meets the body -- and rounded all round once collapsed,
                // since the bar is then the whole window.
                .borderRadius = collapsed ? UICorners(9.0f) : UICorners(9.0f, 9.0f, 0.0f, 0.0f),
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

    // Grows so it pushes the toggle to the far end of the bar.
    UITextConfig titleTextConfig = config.titleTextConfig;
    titleTextConfig.text = name;
    titleTextConfig.style = defaultConfig.titleTextConfig.style;
    addTextLeaf({.width = UISizeSpec::grow()}, titleTextConfig);

    // A sibling of the label rather than part of the bar's own hit area, so toggling and
    // dragging stay separate gestures. The icon points down while open and is turned a
    // quarter to point along the collapsed bar.
    UINodeState collapseButton = openContainer(
        {.width = UISizeSpec::fixed(COLLAPSE_BUTTON_SIZE),
         .height = UISizeSpec::fixed(COLLAPSE_BUTTON_SIZE)},
        {.backgroundColor = Color(0.72f, 0.76f, 0.84f),
         .backgroundImage = dropdownTexture(),
         .imageRotation = collapsed ? (float)PI * 0.5f : 0.0f,
         .onHover = {.backgroundColor = COLOR_WHITE}}
    );
    closeContainer();
    if (collapseButton.isPressed) state.isCollapsed = !collapsed;

    closeContainer();

    dragVec2(titleBar, state.moveDrag, state.pos, POS_UNBOUNDED);

    IdType bodyId = openContainer(defaultConfig.bodyLayout, defaultConfig.bodyStyle).id;

    g_openWindows.push_back({&state, bodyId});
    return window;
}

void closeWindow()
{
    if (g_openWindows.empty()) {
        LOG_ERROR("closeWindow called with no window open; the open/close pairs are unbalanced");
        return;
    }

    UIOpenWindow open = g_openWindows.back();
    UIWindowState& state = *open.state;
    g_openWindows.pop_back();

    closeContainer();

    // Immediate mode gives the window no way to stop the caller declaring content, so a
    // collapsed one discards it here instead -- which also keeps it out of the solve
    // rather than merely hiding it. No grip either: there is nothing to resize.
    if (state.isCollapsed) {
        removeChildren(open.bodyId);
    } else {
        UINodeState grip = dragHandle();
        dragVec2(grip, state.resizeDrag, state.size, WINDOW_MIN);
    }

    closeContainer();
    closeContainer();
}

}   // namespace UIWidgets

}   // namespace Engine
