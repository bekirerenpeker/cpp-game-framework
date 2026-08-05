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

struct UIOpenSection
{
    IdType bodyId = INVALID_ID;
    bool isOpen = false;
};

std::unordered_map<std::string, bool> g_sectionOpen;
std::vector<UIOpenSection> g_openSections;

void dragVec2(const UINodeState& handle, UIDragState& drag, Vec2& value, Vec2 minValue)
{
    if (!handle.isActive) {
        drag.isActive = false;
        return;
    }

    // Unscaled on the way in, so the stored position and size stay in the design units
    // every config field is written in -- the layout scale is applied to them again as
    // the window is declared, and a scaled UI would otherwise drag at the wrong rate.
    Vec2 mouse = unscale(UIRenderer::get().getMouseUiPos());
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
    const UIThemeColors& colors = UITheming::colors();
    const UIThemeMetrics& metrics = UITheming::metrics();

    DragHandleConfig defaultConfig = {
        .handleLayout =
            {
                           .width = UISizeSpec::fixed(metrics.controlHeight),
                           .height = UISizeSpec::fixed(metrics.controlHeight),
                           .floating =
                           UIFloatingConfig {
                           .offset = Vec2(-metrics.spacing.xs, -metrics.spacing.xs),
                           .anchorX = UIAlign::End,
                           .anchorY = UIAlign::End,
                           .selfX = UIAlign::End,
                           .selfY = UIAlign::End
                           },.isFloating = true,
                           },
        // The image is white, so backgroundColor is what tints it.
        .handleStyle = {
                           .backgroundColor = colors.foregroundSubtle,
                           .backgroundImage = dragHandleTexture(),
                           .onHover = {.backgroundColor = colors.foreground},
                           .onHeld = {.backgroundColor = colors.accent},
                           },
    };

    defaultConfig.handleLayout.combine(config.handleLayout);
    defaultConfig.handleStyle.combine(config.handleStyle);

    UINodeState state =
        openContainer(defaultConfig.handleLayout, defaultConfig.handleStyle, config.key);
    closeContainer();

    return state;
}

bool openSection(const std::string& label, const SectionConfig& config)
{
    const UIThemeColors& colors = UITheming::colors();
    const UIThemeMetrics& metrics = UITheming::metrics();

    std::string_view sectionKey = config.key.empty() ? std::string_view(label) : config.key;
    auto found = g_sectionOpen.try_emplace(std::string(sectionKey), config.openByDefault);
    bool& isOpen = found.first->second;

    SectionConfig defaultConfig = {
        .sectionLayout =
            {
                            .width = UISizeSpec::grow(),
                            .gap = metrics.spacing.xs,
                            .direction = UILayoutDirection::Column,
                            },
        .sectionStyle = {},
        .headerLayout =
            {
                            .width = UISizeSpec::grow(),
                            .padding = UIEdges(metrics.spacing.sm, metrics.spacing.xs),
                            .gap = metrics.spacing.sm,
                            .alignCross = UIAlign::Center,
                            },
        .headerStyle =
            {
                            .backgroundColor = colors.surfaceRaised,
                            .borderRadius = metrics.radius.sm,
                            .onHover = {.backgroundColor = colors.surfaceHover},
                            .onHeld = {.backgroundColor = colors.accentMuted},
                            },
        .arrowLayout =
            {
                            .width = UISizeSpec::fixed(metrics.spacing.md),
                            .height = UISizeSpec::fixed(metrics.spacing.md),
                            },
        .arrowStyle =
            {
                            .backgroundColor = colors.foregroundMuted,
                            .backgroundImage = dropdownTexture(),
                            },
        .headerTextConfig = {.style = UITheming::textStyle("h3")},
        // Indented rather than boxed, so nesting a section inside a section reads as
        // depth without stacking a border per level.
        .bodyLayout =
            {
                            .width = UISizeSpec::grow(),
                            .padding =
                    UIEdges(metrics.spacing.md, 0.0f, metrics.spacing.xs, metrics.spacing.sm),
                            .gap = metrics.spacing.sm,
                            .direction = UILayoutDirection::Column,
                            },
        .bodyStyle = {},
    };

    defaultConfig.sectionLayout.combine(config.sectionLayout);
    defaultConfig.sectionStyle.combine(config.sectionStyle);
    defaultConfig.headerLayout.combine(config.headerLayout);
    defaultConfig.headerStyle.combine(config.headerStyle);
    defaultConfig.arrowLayout.combine(config.arrowLayout);
    defaultConfig.arrowStyle.combine(config.arrowStyle);
    defaultConfig.headerTextConfig.style.combine(config.headerTextConfig.style);
    defaultConfig.bodyLayout.combine(config.bodyLayout);
    defaultConfig.bodyStyle.combine(config.bodyStyle);

    openContainer(defaultConfig.sectionLayout, defaultConfig.sectionStyle, sectionKey);

    UINodeState header = openContainer(defaultConfig.headerLayout, defaultConfig.headerStyle);
    // Applied before anything reads it, so the arrow and the body agree within the frame
    // rather than the content lagging the header by one.
    if (header.isPressed) isOpen = !isOpen;

    UIContainerStyleSpec arrowStyle = defaultConfig.arrowStyle;
    arrowStyle.imageRotation = isOpen ? 0.0f : (float)PI * 0.5f;
    openContainer(defaultConfig.arrowLayout, arrowStyle);
    closeContainer();

    UITextConfig headerText = config.headerTextConfig;
    headerText.text = label;
    headerText.style = defaultConfig.headerTextConfig.style;
    addTextLeaf({}, headerText);

    closeContainer();

    // A collapsed section still needs the body container -- the caller's content has to be
    // adopted by something before closeSection can throw it away -- but it has to cost
    // nothing. Emptying it is not enough on its own: a Fit box still measures its own
    // padding, and a child in flow still books the one gap between it and the header.
    // Floating drops it from both the height sum and the gap fence-post, the zero padding
    // collapses what is left to nothing, and ignoreInput stops the invisible leftover
    // from sitting over the header and stealing its hover.
    if (!isOpen) {
        defaultConfig.bodyLayout.isFloating = true;
        defaultConfig.bodyLayout.padding = UIEdges(0.0f);
        defaultConfig.bodyStyle.ignoreInput = true;
    }

    IdType bodyId = openContainer(defaultConfig.bodyLayout, defaultConfig.bodyStyle).id;
    g_openSections.push_back({bodyId, isOpen});

    return isOpen;
}

void closeSection()
{
    if (g_openSections.empty()) {
        LOG_ERROR("closeSection called with no section open; the open/close pairs are unbalanced");
        return;
    }

    UIOpenSection open = g_openSections.back();
    g_openSections.pop_back();

    closeContainer();
    if (!open.isOpen) removeChildren(open.bodyId);
    closeContainer();
}

UINodeState openWindow(const std::string& name, const WindowConfig& config)
{
    const UIThemeColors& colors = UITheming::colors();
    const UIThemeMetrics& metrics = UITheming::metrics();

    std::string_view windowKey = config.key.empty() ? std::string_view(name) : config.key;
    UIWindowState& state = g_windowStates[std::string(windowKey)];
    bool collapsed = state.isCollapsed;

    // Grown to whatever the title style needs rather than pinned to controlHeight: the
    // bar is a Fixed height (the collapsed window is exactly it, so it cannot be a Fit),
    // and a theme with a large heading would otherwise have its own title clipped.
    const UITextStyle& titleStyle = UITheming::textStyle("h1");
    float titleHeight = Math::max(metrics.controlHeight, *titleStyle.size * 1.5f);
    float radius = metrics.radius.lg;

    WindowConfig defaultConfig = {
        .windowLayout =
            {
                           .width = UISizeSpec::fixed(state.size.x),
                           .height = UISizeSpec::fixed(collapsed ? titleHeight : state.size.y),
                           .floating = UIFloatingConfig {.offset = state.pos},
                           .direction = UILayoutDirection::Column,
                           .isFloating = true,
                           },
        .windowStyle =
            {
                           .backgroundColor = colors.background,
                           .borderColor = colors.border,
                           .borderWidth = metrics.borderWidth.thin,
                           .borderRadius = radius,
                           .shadowColor = colors.shadow,
                           .shadowOffset = Vec2(0.0f, metrics.spacing.md),
                           .shadowBlurRadius = metrics.spacing.xl,
                           .overflow = UIOverflow::Hidden,
                           },
        .titleLayout =
            {
                           .width = UISizeSpec::grow(),
                           .height = UISizeSpec::fixed(titleHeight),
                           .padding = UIEdges(metrics.spacing.md, 0.0f),
                           .alignCross = UIAlign::Center,
                           },
        .titleStyle =
            {
                           .backgroundColor = colors.surfaceRaised,
                           // Rounded into the window's own top corners, square where it
                // meets the body -- and rounded all round once collapsed,
                // since the bar is then the whole window.
                .borderRadius =
                    collapsed ? UICorners(radius) : UICorners(radius, radius, 0.0f, 0.0f),
                           .onHover = {.backgroundColor = colors.surfaceHover},
                           .onHeld = {.backgroundColor = colors.accent},
                           },
        .titleTextConfig = {.style = titleStyle},
        .bodyLayout =
            {
                           .width = UISizeSpec::grow(),
                           .height = UISizeSpec::grow(),
                           .padding = UIEdges(metrics.spacing.md),
                           .gap = metrics.spacing.sm,
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
        {.width = UISizeSpec::fixed(metrics.controlHeightSmall),
         .height = UISizeSpec::fixed(metrics.controlHeightSmall)},
        {.backgroundColor = colors.foregroundMuted,
         .backgroundImage = dropdownTexture(),
         .imageRotation = collapsed ? (float)PI * 0.5f : 0.0f,
         .onHover = {.backgroundColor = colors.foreground}}
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
