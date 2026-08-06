#include "ui/widgets/UIDemoWindow.hpp"
#include "ui/UIStateStore.hpp"
#include "ui/widgets/UIWidgets.hpp"
#include "ui/theme/UIThemePresets.hpp"
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

// One swatch per cursor shape, each naming its own, so hovering the strip walks through
// them. The last one sets nothing and picks up its neighbour's -- Default means "no
// opinion", so the strip's own container is what answers for it.
void cursorSwatch(const std::string& label, UICursor cursor)
{
    const UIThemeColors& colors = UITheming::colors();
    const UIThemeMetrics& metrics = UITheming::metrics();

    openContainer(
        {.padding = UIEdges(metrics.spacing.sm, metrics.spacing.xs)},
        {.backgroundColor = colors.surfaceSunken,
         .borderColor = colors.border,
         .borderWidth = metrics.borderWidth.thin,
         .borderRadius = metrics.radius.sm,
         .cursor = cursor,
         .onHover = {.backgroundColor = colors.surfaceHover}}
    );
    text(label);
    closeContainer();
}

void inputTab()
{
    const UIThemeColors& colors = UITheming::colors();
    const UIThemeMetrics& metrics = UITheming::metrics();
    UIStateStore& store = UIStateStore::get();

    static bool rowBlocks = true;
    static int rowClicks = 0;
    static int innerClicks = 0;
    static int doubleClicks = 0;
    static Vec2 wheelTotal = VEC2_ZERO;

    // Everything below is inside this, so scrollDelta answers for the whole tab rather
    // than for whichever box the cursor happens to be over.
    UINodeState tab = openContainer(
        {.width = UISizeSpec::grow(),
         .gap = metrics.spacing.sm,
         .direction = UILayoutDirection::Column}
    );
    wheelTotal = wheelTotal + tab.scrollDelta;

    text("Drag the square. Its position is dragDelta added to where it was on the press,");
    text("so letting go and grabbing it again picks up exactly where it was left.");
    horizontalDivider();

    UINodeState arena = openContainer(
        {.width = UISizeSpec::grow(), .height = UISizeSpec::fixed(150.0f)},
        {.backgroundColor = colors.surfaceSunken,
         .borderColor = colors.borderStrong,
         .borderWidth = 4.0f,
         .borderRadius = metrics.radius.md,
         .overflow = UIOverflow::Hidden},
        "dragArena"
    );

    // Kept on the arena rather than on the square: the offset is an input to the square's
    // own declaration, so it has to be readable before the square exists to have a key.
    Vec2 boxPos = store.getVec2(arena.persistentKey, "boxPos", Vec2(12.0f, 12.0f));

    UINodeState box = openContainer(
        {.width = UISizeSpec::fixed(56.0f),
         .height = UISizeSpec::fixed(56.0f),
         .offset = boxPos,
         .isFloating = true},
        {.backgroundColor = colors.accent,
         .borderColor = colors.accentHover,
         .borderWidth = metrics.borderWidth.thick,
         .borderRadius = metrics.radius.sm,
         .blockInput = true,
         .cursor = UICursor::Move,
         .onHeld = {.backgroundColor = colors.accentActive}},
        "dragBox"
    );
    text("drag");
    closeContainer();

    if (box.isPressed) store.setVec2(box.persistentKey, "boxOrigin", boxPos);
    if (box.isDoubleClicked) doubleClicks++;
    if (box.isActive) {
        // dragDelta is solved pixels and y-up; offset is a config field in design units
        // and y-down, so it needs both conversions or it drags at the wrong rate and the
        // wrong way.
        Vec2 delta = unscale(box.dragDelta);
        boxPos = store.getVec2(box.persistentKey, "boxOrigin") + Vec2(delta.x, -delta.y);
        store.setVec2(arena.persistentKey, "boxPos", boxPos);
    }

    closeContainer();

    text(
        std::format(
            "grabOffset {:.0f}, {:.0f}   dragDelta {:.0f}, {:.0f}   double-clicks {}",
            box.grabOffset.x, box.grabOffset.y, box.dragDelta.x, box.dragDelta.y, doubleClicks
        )
    );

    horizontalDivider();
    text("blockInput: the row below is clickable, and so is the button inside it.");

    checkBox("Row blocks the button's click", rowBlocks);

    UINodeState row = openContainer(
        {.width = UISizeSpec::grow(),
         .padding = UIEdges(metrics.spacing.md),
         .gap = metrics.spacing.md,
         .alignCross = UIAlign::Center},
        {.backgroundColor = colors.surfaceRaised,
         .borderRadius = metrics.radius.sm,
         .onHover = {.backgroundColor = colors.surfaceHover},
         .onHeld = {.backgroundColor = colors.accentMuted}}
    );
    text("Clickable row");
    // Overriding what button() sets for itself, since the default is exactly the
    // behaviour under test: with it off the press carries on up and the row sees it too.
    UINodeState inner = button("Button inside", {.buttonStyle = {.blockInput = rowBlocks}});
    closeContainer();

    if (row.isPressed) rowClicks++;
    if (inner.isPressed) innerClicks++;

    text(std::format("row presses {}   button presses {}", rowClicks, innerClicks));
    text("With it on the counts move apart; with it off every button press bumps both.");

    horizontalDivider();
    text("Cursors, taken from the innermost node that names one:");

    openContainer({.width = UISizeSpec::grow(), .gap = metrics.spacing.sm});
    cursorSwatch("Pointer", UICursor::Pointer);
    cursorSwatch("Text", UICursor::Text);
    cursorSwatch("Crosshair", UICursor::Crosshair);
    cursorSwatch("ResizeNS", UICursor::ResizeNS);
    cursorSwatch("NotAllowed", UICursor::NotAllowed);
    closeContainer();

    horizontalDivider();
    // A wheel tick is non-zero for only the frame or two it lasts, so the instantaneous
    // value reads as a permanent zero however hard you scroll -- the running total is the
    // half that shows it arriving. Shift+wheel drives x.
    text(
        std::format(
            "scrollDelta now {:.1f}, {:.1f}   accumulated {:.0f}, {:.0f}", tab.scrollDelta.x,
            tab.scrollDelta.y, wheelTotal.x, wheelTotal.y
        )
    );

    closeContainer();
}

void spanCell(const std::string& label, uint span, Color color)
{
    const UIThemeColors& colors = UITheming::colors();
    const UIThemeMetrics& metrics = UITheming::metrics();

    openContainer(
        {.padding = UIEdges(metrics.spacing.sm, metrics.spacing.xs),
         .gridSpan = span,
         .alignMain = UIAlign::Center},
        {.backgroundColor = color,
         .borderColor = colors.border,
         .borderWidth = metrics.borderWidth.thin,
         .borderRadius = metrics.radius.sm}
    );
    text(label);
    closeContainer();
}

// Cells are just the children, in order -- nothing here marks where one ends. The three
// grids are the three shapes worth knowing: spans over the default twelve, tracks written
// out so one column can hug its content, and a plain count for a uniform field.
void gridTab()
{
    const UIThemeColors& colors = UITheming::colors();
    const UIThemeMetrics& metrics = UITheming::metrics();

    static int quality = 2;
    static bool longLabel = true;

    text("Twelve columns unless told otherwise, and a cell takes some of them.");
    horizontalDivider();

    openGrid();
    spanCell("3", 3, colors.accentMuted);
    spanCell("9", 9, colors.surfaceRaised);
    spanCell("4", 4, colors.surfaceRaised);
    spanCell("4", 4, colors.accentMuted);
    spanCell("4", 4, colors.surfaceRaised);
    spanCell("5", 5, colors.accentMuted);
    spanCell("8 -- would not fit beside the 5, so it wrapped", 8, colors.surfaceRaised);
    closeGrid();

    horizontalDivider();
    text("Written-out tracks. The fit column is as wide as the widest label in it,");
    text("so the second column starts at the same x on every row.");

    openGrid({
        .columns = {        UISizeSpec::fit(),            UISizeSpec::grow()},
        .gridLayout = {.gap = metrics.spacing.md, .alignCross = UIAlign::Center}
    });
    text("Name");
    button("Player One");
    text("Texture quality");
    dropdown({"Low", "Medium", "High"}, quality);
    text("A deliberately long label");
    checkBox("drags the first column out with it", longLabel);
    closeGrid();

    horizontalDivider();
    text("Six equal columns. The cells fill their column and set their own height.");

    openGrid({.columns = 6, .gridLayout = {.gap = metrics.spacing.xs}});
    for (int i = 0; i < 18; i++) {
        openContainer(
            {.height = UISizeSpec::fixed(38.0f),
             .alignMain = UIAlign::Center,
             .alignCross = UIAlign::Center},
            {.backgroundColor = colors.surfaceSunken,
             .borderColor = colors.border,
             .borderWidth = metrics.borderWidth.thin,
             .borderRadius = metrics.radius.sm,
             .onHover = {.backgroundColor = colors.surfaceHover}}
        );
        text(std::format("{}", i));
        closeContainer();
    }
    closeGrid();
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

    toolbarMenu(
        {"Widgets", "Display", "Sliders", "Color", "Theme", "Scroll", "Input", "Grid"}, selectedMenu
    );

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

    case 6: inputTab(); break;

    case 7: gridTab(); break;

    default: break;
    }

    closeWindow();
}

}   // namespace UIWidgets

}   // namespace Engine
