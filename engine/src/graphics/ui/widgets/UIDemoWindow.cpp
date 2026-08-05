#include "graphics/ui/widgets/UIDemoWindow.hpp"
#include "graphics/ui/widgets/UIWidgets.hpp"
#include "graphics/ui/theme/UIThemePresets.hpp"
#include <format>
#include <string>
#include <vector>

namespace Engine {

namespace UIWidgets {

namespace {

// One editable text style. Only the two fields worth a slider and a swatch -- the point
// is that a theme's text styles are editable at all, not that every field is exposed.
struct DemoTextStyleEdit
{
    std::string name;
    float size = 14.0f;
    Color color = COLOR_WHITE;
};

// The seeds and ramp steps the editor exposes, held concrete because a slider needs a
// float& and a picker a Color&, while the theme's own colour fields are optional.
struct DemoThemeEdit
{
    Color background;
    Color accent;
    Color foreground;
    float scale = 1.0f;
    float radius = 6.0f;
    float spacing = 10.0f;
    float borderWidth = 1.0f;
    std::vector<DemoTextStyleEdit> textStyles;
};

// The preset is kept whole and the edits are laid over a copy of it, so a preset that
// names more than the three seeds -- Brutalist's square corners, Amber CRT's tinted
// border -- keeps them while the seeds stay editable.
UITheme g_baseTheme;
DemoThemeEdit g_edit;
int g_presetIndex = 0;
bool g_initialized = false;

void readBackFromTheme()
{
    const UIThemeColors& colors = UITheming::colors();
    const UIThemeMetrics& metrics = UITheming::metrics();

    g_edit.background = colors.background;
    g_edit.accent = colors.accent;
    g_edit.foreground = colors.foreground;
    g_edit.scale = metrics.scale;
    g_edit.radius = metrics.radius.md;
    g_edit.spacing = metrics.spacing.md;
    g_edit.borderWidth = metrics.borderWidth.thin;

    g_edit.textStyles.clear();
    for (const char* name : {"h1", "h3", "body", "label"}) {
        const UITextStyle& style = UITheming::textStyle(name);
        g_edit.textStyles.push_back({name, *style.size, *style.color});
    }
}

void loadPreset(int index)
{
    const std::vector<UIThemePreset>& presets = getThemePresets();
    if (index < 0 || index >= (int)presets.size()) return;

    g_baseTheme = presets[index].theme;
    UIThemeManager::get().setTheme(g_baseTheme);
    readBackFromTheme();
}

// Applied at the top of the frame rather than where the sliders are, so the whole tree is
// built against one theme -- changing it mid-build would style half the window with the
// old values and half with the new.
void applyEdits()
{
    // The first pass has to load a preset before it can push one, or the edit state's
    // default-constructed (black) colours would be applied as a theme for one frame.
    if (!g_initialized) {
        g_initialized = true;
        loadPreset(g_presetIndex);
    }

    UITheme theme = g_baseTheme;
    theme.colors.background = g_edit.background;
    theme.colors.accent = g_edit.accent;
    theme.colors.foreground = g_edit.foreground;

    theme.metrics.scale = g_edit.scale;
    theme.metrics.radius.md = g_edit.radius;
    theme.metrics.radius.sm = g_edit.radius * 0.66f;
    theme.metrics.radius.lg = g_edit.radius * 1.5f;
    theme.metrics.spacing.md = g_edit.spacing;
    theme.metrics.spacing.sm = g_edit.spacing * 0.6f;
    theme.metrics.spacing.lg = g_edit.spacing * 1.6f;
    theme.metrics.borderWidth.thin = g_edit.borderWidth;
    theme.metrics.borderWidth.thick = g_edit.borderWidth * 2.0f;

    for (const DemoTextStyleEdit& style : g_edit.textStyles)
        theme.textStyles[style.name] = {.color = style.color, .size = style.size};

    UIThemeManager::get().setTheme(theme);
}

void themeTab()
{
    text("Preset");
    int chosen = g_presetIndex;
    dropdown(getThemePresetNames(), chosen);
    if (chosen != g_presetIndex) {
        g_presetIndex = chosen;
        loadPreset(g_presetIndex);
    }

    horizontalDivider();

    if (openSection("Colours", {.openByDefault = true})) {
        // Only the three seeds: everything else on screen is derived from them, which is
        // the whole point worth demonstrating.
        openContainer({.gap = UITheming::metrics().spacing.md, .alignCross = UIAlign::Center});
        text("Background");
        colorPickerPopup(g_edit.background);
        closeContainer();

        openContainer({.gap = UITheming::metrics().spacing.md, .alignCross = UIAlign::Center});
        text("Accent");
        colorPickerPopup(g_edit.accent);
        closeContainer();

        openContainer({.gap = UITheming::metrics().spacing.md, .alignCross = UIAlign::Center});
        text("Foreground");
        colorPickerPopup(g_edit.foreground);
        closeContainer();
    }
    closeSection();

    if (openSection("Metrics")) {
        static float scale = g_edit.scale;
        const auto& inputState = sliderFloat("Scale", scale, 0.6f, 2.0f);
        if (inputState.isReleased) g_edit.scale = scale;

        sliderFloat("Radius", g_edit.radius, 0.0f, 20.0f, {.decimals = 1});
        sliderFloat("Spacing", g_edit.spacing, 2.0f, 24.0f, {.decimals = 1});
        sliderFloat("Border", g_edit.borderWidth, 0.0f, 5.0f, {.decimals = 1});
    }
    closeSection();

    if (openSection("Text styles")) {
        for (DemoTextStyleEdit& style : g_edit.textStyles) {
            if (openSection(style.name)) {
                sliderFloat("Size", style.size, 8.0f, 42.0f, {.decimals = 1});

                openContainer(
                    {.gap = UITheming::metrics().spacing.md, .alignCross = UIAlign::Center}
                );
                text("Colour");
                colorPickerPopup(style.color);
                closeContainer();
            }
            closeSection();
        }
    }
    closeSection();
}

}   // namespace

// A text leaf takes no input of its own, so anything meant to react to the cursor needs
// a container around it -- which is all a "hoverable label" is.
UINodeState hoverLabel(const std::string& label)
{
    const UIThemeColors& colors = UITheming::colors();
    const UIThemeMetrics& metrics = UITheming::metrics();

    UINodeState state = openContainer(
        {.padding = UIEdges(metrics.spacing.sm, metrics.spacing.xs)},
        {.borderRadius = metrics.radius.sm, .onHover = {.backgroundColor = colors.surfaceHover}}
    );
    text(label);
    closeContainer();
    return state;
}

// The leaf grows to fill a box wider than the text, which is what gives alignment any
// slack to work with -- a shrink-to-fit leaf is its own content and has nowhere to move.
void alignedBox(const std::string& label, TextAlignH align)
{
    const UIThemeColors& colors = UITheming::colors();
    const UIThemeMetrics& metrics = UITheming::metrics();

    openContainer(
        {.width = UISizeSpec::grow(), .padding = UIEdges(metrics.spacing.sm)},
        {.backgroundColor = colors.surfaceSunken, .borderRadius = metrics.radius.sm}
    );
    text(
        label, {.textLayout = {.width = UISizeSpec::grow()},
                .textConfig = {.alignment = {.horizontal = align}}}
    );
    closeContainer();
}

void overflowBox(const std::string& label, TextOverflow overflow)
{
    const UIThemeColors& colors = UITheming::colors();
    const UIThemeMetrics& metrics = UITheming::metrics();

    openContainer({.width = UISizeSpec::grow(), .gap = metrics.spacing.xs});
    text(label, {.textLayout = {.width = UISizeSpec::fixed(60.0f)}});

    openContainer(
        {.width = UISizeSpec::fixed(170.0f), .padding = UIEdges(metrics.spacing.sm)},
        {.backgroundColor = colors.surfaceSunken,
         .borderColor = colors.border,
         .borderWidth = metrics.borderWidth.thin,
         .borderRadius = metrics.radius.sm}
    );
    text(
        "A single line far too long for this box",
        {
            .textConfig = {.overflow = overflow, .wrapEnabled = false}
    }
    );
    closeContainer();
    closeContainer();
}

void displayTab()
{
    const UIThemeMetrics& metrics = UITheming::metrics();

    text("Horizontal alignment");
    horizontalDivider();

    openContainer({.width = UISizeSpec::grow(), .gap = metrics.spacing.sm});
    alignedBox("Left", TextAlignH::Left);
    alignedBox("Center", TextAlignH::Center);
    alignedBox("Right", TextAlignH::Right);
    closeContainer();

    text(
        "Alignment is baked into each run's offset while the width is still final, so it "
        "applies per line rather than to the block -- which is why a wrapped paragraph "
        "centres line by line like this one.",
        {.textLayout = {.width = UISizeSpec::grow()},
         .textConfig = {.alignment = {.horizontal = TextAlignH::Center}}}
    );

    horizontalDivider();
    text("Overflow: Clip cuts at the box, Ellipsis cuts the layout back and marks it");

    overflowBox("Visible", TextOverflow::Visible);
    overflowBox("Clip", TextOverflow::Clip);
    overflowBox("Ellipsis", TextOverflow::Ellipsis);

    horizontalDivider();
    text("Tooltips, on plain labels rather than buttons");

    openContainer({.gap = metrics.spacing.md, .alignCross = UIAlign::Center});
    tooltip(
        "A tooltip declared inside the window, escaping its clip",
        hoverLabel("Follows the cursor").isHovered
    );

    UINodeState above = hoverLabel("Anchored above");
    tooltip(
        "Anchored to the label's top edge and aligned by its own bottom", above.isHovered,
        {.anchor = above.pos + Vec2(above.size.x * 0.5f, 0.0f),
         .anchorOffset = Vec2(0.0f, -6.0f),
         .anchorAlignX = UIAlign::Center,
         .anchorAlignY = UIAlign::End}
    );
    closeContainer();
}

// Nothing here asks for scrolling: the box is a fixed height with more in it than fits,
// and overflow = Scroll is the whole of the opt-in. The long rows do not wrap, so their
// min-content is the whole line and the same box overflows sideways too.
void scrollTab()
{
    const UIThemeColors& colors = UITheming::colors();
    const UIThemeMetrics& metrics = UITheming::metrics();

    text("A fixed-height box with more in it than fits, on both axes.");
    horizontalDivider();

    openContainer(
        {.width = UISizeSpec::grow(),
         .height = UISizeSpec::fixed(200.0f),
         .padding = UIEdges(metrics.spacing.sm),
         .gap = metrics.spacing.xs,
         .direction = UILayoutDirection::Column},
        {.backgroundColor = colors.surfaceSunken,
         .borderColor = colors.border,
         .borderWidth = metrics.borderWidth.thin,
         .borderRadius = metrics.radius.md,
         .overflow = UIOverflow::Scroll}
    );

    for (int i = 0; i < 30; i++) {
        std::string label = std::format("Row {}", i);
        if (i % 7 == 3)
            label += "  -- and a deliberately long unwrapped line to push the box sideways";

        text(label, {.textConfig = {.wrapEnabled = false}});
    }

    closeContainer();

    horizontalDivider();
    text("Wheel scrolls vertically, shift+wheel horizontally, and either bar drags.");
}

void demoWindow()
{
    applyEdits();

    static int selectedMenu = 0;
    static float sliderValue = 0.5f;
    static int stepCount = 3;
    static bool checkedA = true;
    static bool checkedB = false;
    static int radioChoice = 1;
    static int filterChoice = 1;
    static int menuChoice = 0;
    static Color pickedColor(0.30f, 0.62f, 0.95f);
    static Color popupColor(0.95f, 0.55f, 0.25f);

    openWindow("Demo Window");

    toolbarMenu({"Widgets", "Display", "Sliders", "Color", "Theme", "Scroll"}, selectedMenu);

    switch (selectedMenu) {
    case 0: {
        text("Text leaf and simple widgets");
        horizontalDivider();

        button("Button");

        horizontalDivider();

        checkBox("Checkbox A", checkedA);
        checkBox("Checkbox B", checkedB);

        horizontalDivider();

        radioGroup({"First", "Second", "Third"}, radioChoice);

        horizontalDivider();

        openContainer({.gap = 10.0f, .alignCross = UIAlign::Center});
        dropdown({"Nearest", "Bilinear", "Trilinear", "Anisotropic 16x"}, filterChoice);
        dropdown(
            {"New scene", "Open scene", "Save scene as a very long file name"}, menuChoice,
            {.label = "File"}
        );
        closeContainer();
        break;
    }

    case 1: displayTab(); break;

    case 2: {
        text("Sliders");
        horizontalDivider();

        // The point of the return value: isChanged fires all the way through a drag,
        // isReleased once at the end, so an expensive rebuild can wait for the latter.
        static int commits = 0;
        UIInputState slider = sliderFloat("Float", sliderValue, 0.0f, 1.0f);
        if (slider.isReleased) commits++;

        sliderInt("Int", stepCount, 0, 10);

        text(
            std::format(
                "editing: {}    changed this frame: {}    commits on release: {}",
                slider.isEditing ? "yes" : "no", slider.isChanged ? "yes" : "no", commits
            )
        );
        break;
    }

    case 3:
        text("Popup picker");
        colorPickerPopup(popupColor);

        horizontalDivider();

        text("Inline picker");
        colorPicker(pickedColor);
        break;

    case 4: themeTab(); break;

    case 5: scrollTab(); break;

    default: break;
    }

    closeWindow();
}

}   // namespace UIWidgets

}   // namespace Engine
