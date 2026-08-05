#include "graphics/ui/widgets/UIWidgets.hpp"
#include "graphics/ui/widgets/UIWidgetsInternal.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "graphics/ui/UIRenderer.hpp"
#include "graphics/ui/UIStateStore.hpp"
#include "graphics/ui/UiManager.hpp"
#include "utils/math/MathFuncs.hpp"
#include <string>
#include <vector>

namespace Engine {

namespace UIWidgets {

namespace {

const Vec2 WINDOW_MIN(200.0f, 120.0f);
const Vec2 WINDOW_DEFAULT_POS(60.0f, 60.0f);
const Vec2 WINDOW_DEFAULT_SIZE(700.0f, 500.0f);
const Vec2 POS_UNBOUNDED(-UI_UNBOUNDED, -UI_UNBOUNDED);

// The state key rather than a pointer, since closeWindow has to reach the same entries
// openWindow read and the store is addressed by key throughout. The body's id has to
// survive with it: that is where a collapsed window throws away what the caller declared.
struct UIOpenWindow
{
    uint64_t stateKey = 0;
    IdType bodyId = INVALID_ID;
};

std::vector<UIOpenWindow> g_openWindows;

struct UIOpenSection
{
    IdType bodyId = INVALID_ID;
    bool isOpen = false;
};

std::vector<UIOpenSection> g_openSections;

// The origins hang off the handle's own node rather than off whatever is being dragged,
// so a widget no longer has to own a drag struct to be draggable.
void dragVec2(const UINodeState& handle, Vec2& value, Vec2 minValue)
{
    UIStateStore& store = UIStateStore::get();
    UIStateFlag isDragging = store.flag(handle.persistentKey, "dragActive");

    if (!handle.isActive) {
        isDragging = false;
        return;
    }

    // Unscaled on the way in, so the stored position and size stay in the design units
    // every config field is written in -- the layout scale is applied to them again as
    // the window is declared, and a scaled UI would otherwise drag at the wrong rate.
    Vec2 mouse = unscale(UIRenderer::get().getMouseUiPos());
    if (!isDragging) {
        isDragging = true;
        store.setVec2(handle.persistentKey, "dragPointerOrigin", mouse);
        store.setVec2(handle.persistentKey, "dragValueOrigin", value);
    }

    // The mouse is y-up while both the position and the size are y-down.
    Vec2 delta = mouse - store.getVec2(handle.persistentKey, "dragPointerOrigin");
    Vec2 origin = store.getVec2(handle.persistentKey, "dragValueOrigin");
    value =
        Vec2(Math::max(origin.x + delta.x, minValue.x), Math::max(origin.y - delta.y, minValue.y));
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

    // Opened before the state is read, because the section's own node is what the state
    // hangs off: a name alone would collide with the same name under another parent.
    UINodeState section =
        openContainer(defaultConfig.sectionLayout, defaultConfig.sectionStyle, sectionKey);
    UIStateFlag isOpen =
        UIStateStore::get().flag(section.persistentKey, "sectionOpen", config.openByDefault);

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

    // A root's own position is forced to zero and floating is applied by the parent, so a
    // top-level window needs this invisible wrapper to have something to float in.
    // Floating too, so a nested window books no slot in the list around it. Opened first
    // of all because its key is what the stored geometry hangs off, and that geometry is
    // an input to the window's own layout.
    UINodeState wrapper = openContainer({.isFloating = true}, {}, windowKey);

    UIStateStore& store = UIStateStore::get();
    uint64_t stateKey = wrapper.persistentKey;
    Vec2 pos = store.getVec2(stateKey, "windowPos", WINDOW_DEFAULT_POS);
    Vec2 size = store.getVec2(stateKey, "windowSize", WINDOW_DEFAULT_SIZE);
    UIStateFlag collapsed = store.flag(stateKey, "windowCollapsed");

    // Grown to whatever the title style needs rather than pinned to controlHeight: the
    // bar is a Fixed height (the collapsed window is exactly it, so it cannot be a Fit),
    // and a theme with a large heading would otherwise have its own title clipped.
    const UITextStyle& titleStyle = UITheming::textStyle("h1");
    float titleHeight = Math::max(metrics.controlHeight, *titleStyle.size * 1.5f);
    float radius = metrics.radius.lg;

    WindowConfig defaultConfig = {
        .windowLayout =
            {
                           .width = UISizeSpec::fixed(size.x),
                           .height = UISizeSpec::fixed(collapsed ? titleHeight : size.y),
                           .floating = UIFloatingConfig {.offset = pos},
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
    if (collapseButton.isPressed) collapsed = !collapsed;

    closeContainer();

    dragVec2(titleBar, pos, POS_UNBOUNDED);
    store.setVec2(stateKey, "windowPos", pos);

    IdType bodyId = openContainer(defaultConfig.bodyLayout, defaultConfig.bodyStyle).id;

    g_openWindows.push_back({stateKey, bodyId});
    return window;
}

void closeWindow()
{
    if (g_openWindows.empty()) {
        LOG_ERROR("closeWindow called with no window open; the open/close pairs are unbalanced");
        return;
    }

    UIOpenWindow open = g_openWindows.back();
    g_openWindows.pop_back();

    closeContainer();

    // Immediate mode gives the window no way to stop the caller declaring content, so a
    // collapsed one discards it here instead -- which also keeps it out of the solve
    // rather than merely hiding it. No grip either: there is nothing to resize.
    UIStateStore& store = UIStateStore::get();
    if (store.flag(open.stateKey, "windowCollapsed")) {
        removeChildren(open.bodyId);
    } else {
        UINodeState grip = dragHandle();
        Vec2 size = store.getVec2(open.stateKey, "windowSize", WINDOW_DEFAULT_SIZE);
        dragVec2(grip, size, WINDOW_MIN);
        store.setVec2(open.stateKey, "windowSize", size);
    }

    closeContainer();
    closeContainer();
}

}   // namespace UIWidgets

}   // namespace Engine
