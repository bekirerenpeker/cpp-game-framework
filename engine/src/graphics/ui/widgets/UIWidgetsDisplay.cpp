#include "graphics/ui/UIWidgets.hpp"
#include "graphics/ui/UiManager.hpp"

namespace Engine {

namespace UIWidgets {

namespace {

constexpr float DIVIDER_THICKNESS = 1.0f;
const Color DIVIDER_COLOR(0.28f, 0.31f, 0.40f);

}   // namespace

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


}   // namespace UIWidgets

}   // namespace Engine
