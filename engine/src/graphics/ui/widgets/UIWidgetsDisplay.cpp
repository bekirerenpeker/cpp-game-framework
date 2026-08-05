#include "graphics/ui/widgets/UIWidgets.hpp"
#include "graphics/ui/UiManager.hpp"

namespace Engine {

namespace UIWidgets {

UINodeState horizontalDivider(const DividerConfig& config)
{
    const UIThemeColors& colors = UITheming::colors();
    const UIThemeMetrics& metrics = UITheming::metrics();

    DividerConfig defaultConfig = {
        .dividerLayout =
            {
                            .width = UISizeSpec::grow(),
                            .height = UISizeSpec::fixed(metrics.borderWidth.thin + 1),
                            },
        .dividerStyle = {.backgroundColor = colors.border                              },
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
    const UIThemeColors& colors = UITheming::colors();
    const UIThemeMetrics& metrics = UITheming::metrics();

    DividerConfig defaultConfig = {
        .dividerLayout =
            {
                            .width = UISizeSpec::fixed(metrics.borderWidth.thin + 1),
                            .height = UISizeSpec::grow(),
                            },
        .dividerStyle = {                        .backgroundColor = colors.border },
    };

    defaultConfig.dividerLayout.combine(config.dividerLayout);
    defaultConfig.dividerStyle.combine(config.dividerStyle);

    UINodeState state =
        openContainer(defaultConfig.dividerLayout, defaultConfig.dividerStyle, config.key);
    closeContainer();

    return state;
}

IdType text(const std::string label, const TextConfig& config)
{
    TextConfig defaultConfig = {
        .textLayout = {},
        .textConfig = {.style = UITheming::textStyle("body")},
    };

    defaultConfig.textLayout.combine(config.textLayout);
    defaultConfig.textConfig.style.combine(config.textConfig.style);

    UITextConfig textConfig = config.textConfig;
    textConfig.text = label;
    textConfig.style = defaultConfig.textConfig.style;

    return addTextLeaf(defaultConfig.textLayout, textConfig);
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

    const UIThemeColors& colors = UITheming::colors();
    const UIThemeMetrics& metrics = UITheming::metrics();

    TooltipConfig defaultConfig = {
        .tooltipLayout =
            {
                            .padding = UIEdges(metrics.spacing.sm, metrics.spacing.xs),
                            .isFloating = true,
                            },
        .tooltipStyle =
            {
                            .backgroundColor = colors.surfaceOverlay,
                            .borderColor = colors.borderStrong,
                            .borderWidth = metrics.borderWidth.thin,
                            .borderRadius = metrics.radius.sm,
                            .shadowColor = colors.shadow,
                            .shadowOffset = Vec2(0.0f, metrics.spacing.xs),
                            .shadowBlurRadius = metrics.spacing.md,
                            .ignoreClip = true,
                            .zIndex = 64,
                            },
        .labelLayout = {},
        .labelTextConfig = {.style = UITheming::textStyle("label")},
    };

    defaultConfig.tooltipLayout.combine(config.tooltipLayout);
    defaultConfig.tooltipStyle.combine(config.tooltipStyle);
    defaultConfig.labelLayout.combine(config.labelLayout);
    defaultConfig.labelTextConfig.style.combine(config.labelTextConfig.style);

    // A floating offset is relative to the parent. localMousePos already is; an explicit
    // anchor is measured from the root, so it needs the anchor node's own position taken
    // off it -- both end up in the same top-left y-down space. Both are solved geometry,
    // so both come back through unscale into the design units a config field carries. The
    // self alignment is what centres the tooltip on the point rather than hanging it off
    // the corner; the caller cannot do that itself, since it would need the solved size.
    if (config.anchor) {
        defaultConfig.tooltipLayout.floating = UIFloatingConfig {
            .offset = unscale(*config.anchor - anchor.pos) + config.anchorOffset,
            .selfX = config.anchorAlignX,
            .selfY = config.anchorAlignY,
        };
    } else {
        defaultConfig.tooltipLayout.floating =
            UIFloatingConfig {.offset = unscale(anchor.localMousePos) + config.cursorOffset};
    }

    openContainer(defaultConfig.tooltipLayout, defaultConfig.tooltipStyle);

    UITextConfig labelText = config.labelTextConfig;
    labelText.text = label;
    labelText.style = defaultConfig.labelTextConfig.style;
    addTextLeaf(defaultConfig.labelLayout, labelText);

    closeContainer();
    closeContainer();
}

}   // namespace UIWidgets

}   // namespace Engine
