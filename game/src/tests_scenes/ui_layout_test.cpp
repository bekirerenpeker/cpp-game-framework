#include "EngineInclude.hpp"
#include "test_funcs.hpp"

using namespace Engine;

namespace {

const char* FONT_FOLDER = "game/assets/fonts";
const char* FALLBACK_FONT = "C:/Windows/Fonts/segoeui.ttf";

// Two independent roots laid out side by side, each solved and rendered by its own
// begin/draw cycle -- the same thing two real windows or two floating panels would
// do. Layout is unitless; this scene treats one UI unit as one pixel of the design
// and scales into world space at draw time.
constexpr float ROOT_A_WIDTH = 560.0f;
constexpr float ROOT_A_HEIGHT = 1440.0f;
constexpr float ROOT_B_WIDTH = 560.0f;
constexpr float ROOT_B_HEIGHT = 1660.0f;
constexpr float ROOT_GAP = 60.0f;
constexpr float WORLD_PER_UI = 0.01f;
constexpr float ROOT_MIN_WIDTH = 260.0f;
constexpr float ROOT_MIN_HEIGHT = 300.0f;
// Narrower than a root's 16 unit padding, so the grab band never overlaps a child and
// the root always wins the hit there.
constexpr float ROOT_GRAB = 14.0f;

fs::path findFontFile()
{
    if (FileManager::get().doesPathExist(FONT_FOLDER)) {
        for (const auto& entry : fs::directory_iterator(FONT_FOLDER)) {
            std::string ext = entry.path().extension().string();
            if (ext == ".ttf" || ext == ".otf") return entry.path();
        }
    }

    LOG_WARNING("no .ttf/.otf found in {}; falling back to {}", FONT_FOLDER, FALLBACK_FONT);
    return FALLBACK_FONT;
}

LayoutConfig makeRow(float gap, const LayoutEdges& padding)
{
    LayoutConfig config;
    config.direction = LayoutDirection::Row;
    config.gap = gap;
    config.padding = padding;
    return config;
}

LayoutConfig makeColumn(float gap, const LayoutEdges& padding)
{
    LayoutConfig config;
    config.direction = LayoutDirection::Column;
    config.gap = gap;
    config.padding = padding;
    return config;
}

LayoutConfig makeBox(float width, float height)
{
    LayoutConfig config;
    config.width = SizeSpec::fixed(width);
    config.height = SizeSpec::fixed(height);
    return config;
}

// Node indices of the cases checked numerically rather than only by eye. Node index
// equals element index, so a builder's return value indexes straight into the
// solved node array -- but only until the next begin(), hence one set per root.
struct LayoutProbes
{
    uint fitRow = NO_NODE;
    uint growA = NO_NODE;
    uint growB = NO_NODE;
    uint cappedA = NO_NODE;
    uint cappedB = NO_NODE;
    uint fitParent = NO_NODE;
    uint fitGrowShort = NO_NODE;
    uint fitGrowLong = NO_NODE;
    uint deepInner = NO_NODE;
    uint alignBox[3] = {NO_NODE, NO_NODE, NO_NODE};
    uint alignChild[3] = {NO_NODE, NO_NODE, NO_NODE};
    uint floatParent = NO_NODE;
    uint floatChild = NO_NODE;
    uint image = NO_NODE;
    uint overflowRow = NO_NODE;
    uint overflowFirst = NO_NODE;
};

struct TextProbes
{
    uint wrapNarrow = NO_NODE;
    uint wrapText = NO_NODE;
    uint styledLines = NO_NODE;
    uint styledWrap = NO_NODE;
    uint fixedLine = NO_NODE;
};

struct DividerProbes
{
    uint thin = NO_NODE;
    uint thick = NO_NODE;
    uint rounded = NO_NODE;
};

}   // namespace

// Exercises the five-pass layout solver across two separate roots, drawing every
// computed box and the text laid out inside it. Boxes are tinted by tree depth and
// floating nodes are red. WASD pans, Q/E zooms, Tab swaps the mtsdf/bitmap atlas,
// Space hides the boxes. The box tree must be identical at every zoom, since
// layout is unitless.
int ui_layout_test()
{
    IdType windowId = WindowManager::get().createWindow({900, 900, "UI Layout Test"});

    Registry registry;
    EntityHandle camera = registry.create();
    camera.emplace<TransformComponent>();
    camera.emplace<CameraComponent>().windowId = windowId;

    fs::path fontPath = findFontFile();
    Font mtsdfFont(fontPath, {.atlasType = FontAtlasType::Mtsdf, .emPixelSize = 48});
    Font bitmapFont(fontPath, {.atlasType = FontAtlasType::Bitmap, .emPixelSize = 32});
    if (!mtsdfFont.isValid()) {
        LOG_ERROR("no usable font; text leaves would all measure as empty");
        return 1;
    }

    GlShader quadShader("game/assets/shaders/QuadShader.glsl");
    GlShader textShader("game/assets/shaders/TextShader.glsl");
    GlShader uiShader("game/assets/shaders/UiShader.glsl");
    Renderer::get().init(6000, &quadShader);
    TextRenderer::get().init(&textShader, 8000);
    UiRenderer::get().init(&uiShader, 2000);

    Input::get().addAxis("Horizontal", {KeyCode::D, KeyCode::A});
    Input::get().addAxis("Vertical", {KeyCode::W, KeyCode::S});
    Input::get().addAxis("Zoom", {KeyCode::E, KeyCode::Q});

    const TextStyle bodyStyle {.color = COLOR_WHITE, .size = 22.0f};
    const TextStyle labelStyle {.color = Color(0.62f, 0.66f, 0.74f), .size = 17.0f};
    const TextStyle titleStyle {.color = Color(1.0f, 0.85f, 0.45f), .size = 28.0f};

    // The span styles the /s tags in the styled cases resolve against.
    const TextStyle bigStyle {.color = COLOR_CYAN, .size = 46.0f};
    const TextStyle smallStyle {.color = Color(1.0f, 0.7f, 0.3f), .size = 13.0f};
    const std::vector<TextStyle> mixedStyles {bigStyle, smallStyle};

    UiSystem& ui = UiSystem::get();
    ui.setDefaultFont(&mtsdfFont);
    ui.setDefaultTextStyle(bodyStyle);

    // The UI lives in window space now, so the camera no longer moves it -- WASD/QE pan
    // and zoom the UI viewport itself, which is what reaches the cases below the fold.
    const float totalWidth = ROOT_A_WIDTH + ROOT_GAP + ROOT_B_WIDTH;
    float uiScale = 1.0f;
    Vec2 uiPan(8.0f, 8.0f);
    bool viewportFitted = false;

    // Overrides are deliberately style-only: a size or padding override could grow the
    // element out from under the cursor and flicker between the two states at 60Hz.
    UiStyles buttonStyles;
    buttonStyles.normal.backgroundColor = Color(0.18f, 0.20f, 0.26f);
    buttonStyles.normal.borderColor = Color(0.35f, 0.38f, 0.46f);
    buttonStyles.normal.borderWidth = 1.0f;
    buttonStyles.hovered.backgroundColor = Color(0.28f, 0.32f, 0.42f);
    buttonStyles.hovered.borderColor = Color(0.60f, 0.66f, 0.80f);
    buttonStyles.pressed.backgroundColor = Color(0.10f, 0.11f, 0.15f);
    buttonStyles.pressed.contentColor = COLOR_YELLOW;

    UiStyles panelStyles = UiPresets::panel();
    panelStyles.hovered.borderColor = Color(0.95f, 0.75f, 0.35f);
    panelStyles.hovered.borderWidth = 2.0f;
    panelStyles.pressed.backgroundColor = Color(0.20f, 0.24f, 0.30f);

    // The demo boxes carry a real style now that there is no debug outline pass; the
    // caseBlock columns around them stay unstyled, which is the layout-only case.
    UiStyles boxStyles = UiPresets::outline(Color(0.45f, 0.72f, 1.00f));
    boxStyles.normal.backgroundColor = Color(0.45f, 0.72f, 1.00f, 0.10f);
    boxStyles.hovered.backgroundColor = Color(0.45f, 0.72f, 1.00f, 0.28f);

    UiStyles floaterStyles = UiPresets::outline(Color(1.00f, 0.35f, 0.35f));
    floaterStyles.normal.backgroundColor = Color(1.00f, 0.35f, 0.35f, 0.14f);

    UiStyles caseStyles;
    caseStyles.normal.borderColor = Color(0.30f, 0.34f, 0.42f);
    caseStyles.normal.borderWidth = 1.0f;
    caseStyles.normal.cornerRadius = 6.0f;

    // A visible edge for the root, since its grab band is otherwise just empty padding.
    UiStyles rootStyles;
    rootStyles.normal.borderColor = Color(0.30f, 0.33f, 0.40f);
    rootStyles.normal.borderWidth = 2.0f;
    rootStyles.pressed.borderColor = Color(0.95f, 0.75f, 0.35f);
    rootStyles.pressed.borderWidth = 3.0f;

    // A rule thick enough for the radius to read; the shader clamps it to the shorter
    // half-extent, so anything at or above half the thickness draws the same capsule.
    UiStyles roundedDividerStyles = UiPresets::divider(Color(0.95f, 0.75f, 0.35f));
    roundedDividerStyles.normal.cornerRadius = 4.0f;
    roundedDividerStyles.hovered.backgroundColor = COLOR_WHITE;

    LayoutProbes layoutProbes;
    TextProbes textProbes;
    DividerProbes dividerProbes;
    Vec2 rootASize(ROOT_A_WIDTH, ROOT_A_HEIGHT);
    Vec2 rootBSize(ROOT_B_WIDTH, ROOT_B_HEIGHT);
    Vec2 resizableSize(220.0f, 90.0f);
    int clickCount = 0;
    bool grabbedCorner = false;
    bool logged = false;
    bool inputSettled = false;
    bool showBitmap = false;
    bool showText = true;

    // Logged on change rather than per frame, so the scene reports what it is doing
    // without drowning the console at 60Hz.
    std::unordered_map<std::string, uint> reportedFlags;
    auto reportInteraction = [&](const std::string& name, const UiState& state) {
        uint flags = (state.isHovered ? 1u : 0u) | (state.isHoveredDirect ? 2u : 0u) |
                     (state.isHeld ? 4u : 0u) | (state.isDragging ? 8u : 0u) |
                     (state.hasRect ? 16u : 0u);
        if (state.isClicked)
            LOG_INFO(
                "interaction: CLICK {} at local ({}, {})", name, state.mouseLocal.x,
                state.mouseLocal.y
            );
        auto reported = reportedFlags.find(name);
        if (reported != reportedFlags.end() && reported->second == flags) return;

        reportedFlags[name] = flags;

        // The element's centre in window pixels, so the rect can be checked against
        // where the cursor actually has to be rather than only against layout units.
        Vec2 center = state.pos + state.size * 0.5f;
        Vec2 client =
            UiRenderer::get().getScreenTopLeft() + center * UiRenderer::get().getPixelsPerUiUnit();

        LOG_INFO(
            "interaction: {} hovered {} direct {} held {} dragging {} rect ({}, {}) {}x{} client "
            "({}, {})",
            name, state.isHovered, state.isHoveredDirect, state.isHeld, state.isDragging,
            state.pos.x, state.pos.y, state.size.x, state.size.y, client.x, client.y
        );
    };

    // Drag a root's right or bottom edge. The root is the one element whose size is a
    // plain argument rather than something the solver derives, so resizing it is just
    // editing that argument -- and shrinking the width is what re-wraps every text
    // block inside it, which is the property the whole solver is built around.
    // Latched on the press: once the box moves, the cursor is no longer on the band.
    bool grabbedRootX[2] = {false, false};
    bool grabbedRootY[2] = {false, false};
    auto dragRootEdges = [&](int slot, const UiState& root, Vec2& size) {
        if (root.isPressed) {
            grabbedRootX[slot] = root.mouseLocal.x > root.size.x - ROOT_GRAB;
            grabbedRootY[slot] = root.mouseLocal.y > root.size.y - ROOT_GRAB;
        }
        if (!root.isHeld) return;

        if (grabbedRootX[slot]) size.x = Math::max(ROOT_MIN_WIDTH, size.x + root.mouseDelta.x);
        if (grabbedRootY[slot]) size.y = Math::max(ROOT_MIN_HEIGHT, size.y + root.mouseDelta.y);
    };

    auto caseBlock = [&](const char* title) {
        ui.openContainer(makeColumn(4.0f, LayoutEdges(0.0f)));
        ui.addText(title, labelStyle);
    };

    auto buildLayoutRoot = [&]() {
        UiState root =
            ui.begin(rootASize, makeColumn(14.0f, LayoutEdges(16.0f)), rootStyles, "rootA");
        dragRootEdges(0, root, rootASize);
        reportInteraction("rootA", root);
        ui.addText("ROOT A - sizing, alignment, floating, overflow", titleStyle);

        // 1 -- a Fit row hugs its children plus padding and gaps.
        caseBlock("1  fit row: hugs 3 fixed children + padding + gaps");
        layoutProbes.fitRow = ui.openContainer(makeRow(8.0f, LayoutEdges(8.0f)), caseStyles).index;
        ui.openContainer(makeBox(60.0f, 30.0f), boxStyles);
        ui.closeContainer();
        ui.openContainer(makeBox(90.0f, 30.0f), boxStyles);
        ui.closeContainer();
        ui.openContainer(makeBox(40.0f, 30.0f), boxStyles);
        ui.closeContainer();
        ui.closeContainer();
        ui.closeContainer();

        // 2 -- surplus goes only to the growers, and they end up equal.
        caseBlock("2  fixed | grow | grow: the two growers end equal");
        {
            LayoutConfig config = makeRow(8.0f, LayoutEdges(8.0f));
            config.width = SizeSpec::fixed(440.0f);
            ui.openContainer(config, caseStyles);

            ui.openContainer(makeBox(100.0f, 30.0f), boxStyles);
            ui.closeContainer();

            LayoutConfig grower;
            grower.width = SizeSpec::grow();
            grower.height = SizeSpec::fixed(30.0f);
            layoutProbes.growA = ui.openContainer(grower, boxStyles).index;
            ui.closeContainer();
            layoutProbes.growB = ui.openContainer(grower, boxStyles).index;
            ui.closeContainer();
            ui.closeContainer();
        }
        ui.closeContainer();

        // 3 -- a grower is still bounded by its own max; the leftover is alignMain's.
        caseBlock("3  grow capped by sizing.max: both stop at 120, leftover centred");
        {
            LayoutConfig config = makeRow(8.0f, LayoutEdges(8.0f));
            config.width = SizeSpec::fixed(440.0f);
            config.alignMain = LayoutAlign::Center;
            ui.openContainer(config, caseStyles);

            LayoutConfig capped;
            capped.width = SizeSpec::grow();
            capped.width.max = 120.0f;
            capped.height = SizeSpec::fixed(30.0f);
            layoutProbes.cappedA = ui.openContainer(capped, boxStyles).index;
            ui.closeContainer();
            layoutProbes.cappedB = ui.openContainer(capped, boxStyles).index;
            ui.closeContainer();
            ui.closeContainer();
        }
        ui.closeContainer();

        // 4 -- two growers in a Fit parent must keep their own content widths rather
        // than splitting evenly, or the long one wraps while the short one has slack.
        caseBlock("4  two growers in a fit parent: each keeps its own content width");
        {
            layoutProbes.fitParent =
                ui.openContainer(makeRow(8.0f, LayoutEdges(8.0f)), caseStyles).index;

            LayoutConfig grower;
            grower.width = SizeSpec::grow();
            layoutProbes.fitGrowShort = ui.addText("short", bodyStyle, grower).index;
            layoutProbes.fitGrowLong =
                ui.addText("a much longer label here", bodyStyle, grower).index;
            ui.closeContainer();
        }
        ui.closeContainer();

        // 5 -- nesting three levels deep, alternating direction.
        caseBlock("5  nested row > column > row, 3 levels");
        {
            ui.openContainer(makeRow(8.0f, LayoutEdges(8.0f)));
            for (int outer = 0; outer < 2; outer++) {
                ui.openContainer(makeColumn(6.0f, LayoutEdges(6.0f)));
                for (int inner = 0; inner < 2; inner++) {
                    ui.openContainer(makeRow(4.0f, LayoutEdges(4.0f)));
                    uint first = ui.openContainer(makeBox(50.0f, 22.0f), boxStyles).index;
                    ui.closeContainer();
                    ui.openContainer(makeBox(70.0f, 22.0f), boxStyles);
                    ui.closeContainer();
                    ui.closeContainer();
                    if (outer == 0 && inner == 0) layoutProbes.deepInner = first;
                }
                ui.closeContainer();
            }
            ui.closeContainer();
        }
        ui.closeContainer();

        // 6 -- main-axis and cross-axis alignment.
        caseBlock("6  alignment: main start/centre/end, cross start/centre/end");
        {
            ui.openContainer(makeRow(12.0f, LayoutEdges(0.0f)));
            const LayoutAlign aligns[] = {
                LayoutAlign::Start, LayoutAlign::Center, LayoutAlign::End
            };
            for (int i = 0; i < 3; i++) {
                LayoutConfig config = makeRow(0.0f, LayoutEdges(6.0f));
                config.width = SizeSpec::fixed(160.0f);
                config.height = SizeSpec::fixed(70.0f);
                config.alignMain = aligns[i];
                config.alignCross = aligns[i];
                layoutProbes.alignBox[i] = ui.openContainer(config, caseStyles).index;
                layoutProbes.alignChild[i] =
                    ui.openContainer(makeBox(50.0f, 24.0f), boxStyles).index;
                ui.closeContainer();
                ui.closeContainer();
            }
            ui.closeContainer();
        }
        ui.closeContainer();

        // 7 -- a floating child must not inflate its parent or consume a gap.
        caseBlock("7  floating child (red): parent width matches case 1's row exactly");
        {
            layoutProbes.floatParent =
                ui.openContainer(makeRow(8.0f, LayoutEdges(8.0f)), caseStyles).index;
            ui.openContainer(makeBox(60.0f, 30.0f), boxStyles);
            ui.closeContainer();
            ui.openContainer(makeBox(90.0f, 30.0f), boxStyles);
            ui.closeContainer();
            ui.openContainer(makeBox(40.0f, 30.0f), boxStyles);
            ui.closeContainer();

            LayoutConfig floater = makeBox(70.0f, 20.0f);
            floater.isFloating = true;
            floater.floating.anchorX = LayoutAlign::End;
            floater.floating.anchorY = LayoutAlign::End;
            floater.floating.selfX = LayoutAlign::Center;
            floater.floating.selfY = LayoutAlign::Center;
            layoutProbes.floatChild = ui.openContainer(floater, floaterStyles).index;
            ui.closeContainer();
            ui.closeContainer();
        }
        ui.closeContainer();

        // 8 -- a non-text leaf whose height also depends on its final width.
        caseBlock("8  image, aspect 2:1, grow width: height tracks the final width");
        {
            LayoutConfig config = makeRow(0.0f, LayoutEdges(6.0f));
            config.width = SizeSpec::fixed(380.0f);
            ui.openContainer(config, caseStyles);

            LayoutConfig imageConfig;
            imageConfig.width = SizeSpec::grow();
            layoutProbes.image =
                ui.addImage(mtsdfFont.getTexture(), Vec2(200.0f, 100.0f), 2.0f, imageConfig).index;
            ui.closeContainer();
        }
        ui.closeContainer();

        // 9 -- every child at its minimum and still not fitting is real overflow.
        caseBlock("9  overflow: three 80-wide children in a 150-wide row");
        {
            LayoutConfig config = makeRow(6.0f, LayoutEdges(6.0f));
            config.width = SizeSpec::fixed(150.0f);
            layoutProbes.overflowRow = ui.openContainer(config, caseStyles).index;
            layoutProbes.overflowFirst = ui.openContainer(makeBox(80.0f, 26.0f), boxStyles).index;
            ui.closeContainer();
            ui.openContainer(makeBox(80.0f, 26.0f), boxStyles);
            ui.closeContainer();
            ui.openContainer(makeBox(80.0f, 26.0f), boxStyles);
            ui.closeContainer();
            ui.closeContainer();
        }
        ui.closeContainer();

        ui.draw();
    };

    auto buildTextRoot = [&]() {
        UiState root =
            ui.begin(rootBSize, makeColumn(14.0f, LayoutEdges(16.0f)), rootStyles, "rootB");
        dragRootEdges(1, root, rootBSize);
        reportInteraction("rootB", root);
        ui.addText("ROOT B - interaction, wrapping, styled spans, line height", titleStyle);

        // 0 -- interaction. Every state is read from last frame's rects, so it is known
        // before the element is pushed and the style override resolves inline. First in
        // the root rather than last, because it is the one case the mouse has to reach
        // without panning the camera off it.
        caseBlock("0  interaction: hover/press styles, inline clicks, drag to resize");
        {
            ui.openContainer(makeColumn(10.0f, LayoutEdges(0.0f)));

            ui.openContainer(makeRow(10.0f, LayoutEdges(0.0f)));
            UiState count = ui.addButton("Count", {}, buttonStyles);
            UiState reset = ui.addButton("Reset", {}, buttonStyles);
            if (count.isClicked) clickCount++;
            if (reset.isClicked) clickCount = 0;
            ui.addText(std::format("clicks: {}", clickCount), bodyStyle);
            ui.closeContainer();
            reportInteraction("Count", count);
            reportInteraction("Reset", reset);

            LayoutConfig panelConfig = makeRow(0.0f, LayoutEdges(10.0f));
            panelConfig.width = SizeSpec::fixed(resizableSize.x);
            panelConfig.height = SizeSpec::fixed(resizableSize.y);
            UiState panel = ui.openContainer("resizable", panelConfig, panelStyles);
            ui.addText(panel.isHeld ? "resizing" : "drag my bottom-right corner", labelStyle);
            ui.closeContainer();
            reportInteraction("resizable", panel);

            // The engine never writes a size back into LayoutConfig -- it reports the drag
            // in layout units and the scene owns the value. Latched on the press, because
            // the cursor leaves the corner as soon as the box grows under it.
            constexpr float GRAB = 18.0f;
            if (panel.isPressed)
                grabbedCorner = panel.mouseLocal.x > panel.size.x - GRAB &&
                                panel.mouseLocal.y > panel.size.y - GRAB;
            if (panel.isHeld && grabbedCorner) {
                resizableSize.x = Math::max(120.0f, resizableSize.x + panel.mouseDelta.x);
                resizableSize.y = Math::max(60.0f, resizableSize.y + panel.mouseDelta.y);
            }

            ui.closeContainer();
        }
        ui.closeContainer();

        // 10 -- shrinking is what drives the wrap, and the box grows taller for it.
        caseBlock("10  wrap: same text at 230 / 150 / 95 wide");
        {
            ui.openContainer(makeRow(12.0f, LayoutEdges(0.0f)));
            const char* sample = "The quick brown fox jumps over the lazy dog near the riverbank";
            const float widths[] = {230.0f, 150.0f, 95.0f};
            for (int i = 0; i < 3; i++) {
                LayoutConfig column = makeColumn(0.0f, LayoutEdges(6.0f));
                column.width = SizeSpec::fixed(widths[i]);
                uint index = ui.openContainer(column).index;

                LayoutConfig textConfig;
                textConfig.width = SizeSpec::grow();
                uint textIndex = ui.addText(sample, bodyStyle, textConfig).index;
                ui.closeContainer();

                if (i == 2) {
                    textProbes.wrapNarrow = index;
                    textProbes.wrapText = textIndex;
                }
            }
            ui.closeContainer();
        }
        ui.closeContainer();

        // 11 -- a line is as tall as the tallest style on it, so an oversized span
        // pushes its own line down without colliding with the one below.
        caseBlock("11  styled spans: the big span sets its line's height");
        {
            LayoutConfig config = makeColumn(0.0f, LayoutEdges(6.0f));
            config.width = SizeSpec::fixed(500.0f);
            ui.openContainer(config);

            LayoutConfig textConfig;
            textConfig.width = SizeSpec::grow();
            textProbes.styledLines = ui.addText(
                                           "first line is plain\nsecond has a /sBIG/s word in it\n"
                                           "third /stiny/s and plain\nfourth line is plain again",
                                           bodyStyle, mixedStyles, textConfig
            )
                                         .index;
            ui.closeContainer();
        }
        ui.closeContainer();

        // 12 -- the same styled text wrapped narrow: line heights now vary per line
        // and a word is still allowed to straddle a span boundary.
        caseBlock("12  styled + wrapped: per-line heights vary down the block");
        {
            ui.openContainer(makeRow(12.0f, LayoutEdges(0.0f)));
            const float widths[] = {300.0f, 200.0f};
            for (int i = 0; i < 2; i++) {
                LayoutConfig column = makeColumn(0.0f, LayoutEdges(6.0f));
                column.width = SizeSpec::fixed(widths[i]);
                ui.openContainer(column);

                LayoutConfig textConfig;
                textConfig.width = SizeSpec::grow();
                uint index =
                    ui.addText(
                          "the quick /sbrown/s fox jumps over the /slazy/s dog and keeps running "
                          "along the riverbank",
                          bodyStyle, mixedStyles, textConfig
                    )
                        .index;
                ui.closeContainer();
                if (i == 1) textProbes.styledWrap = index;
            }
            ui.closeContainer();
        }
        ui.closeContainer();

        // 13 -- the fixed-line-height escape hatch: same text, uniform steps, so the
        // big span overlaps on purpose instead of pushing its line apart.
        caseBlock("13  same text with fixedLineHeight: uniform steps, big span overlaps");
        {
            LayoutConfig config = makeColumn(0.0f, LayoutEdges(6.0f));
            config.width = SizeSpec::fixed(500.0f);
            ui.openContainer(config);

            LayoutConfig textConfig;
            textConfig.width = SizeSpec::grow();
            textProbes.fixedLine = ui.addText(
                                         "first line is plain\nsecond has a /sBIG/s word in it\n"
                                         "third /stiny/s and plain\nfourth line is plain again",
                                         bodyStyle, mixedStyles, textConfig, true, true
            )
                                       .index;
            ui.closeContainer();
        }
        ui.closeContainer();

        // 14 -- a button is a padded box wrapping its own label leaf.
        caseBlock("14  buttons: each hugs its label plus the default padding");
        {
            ui.openContainer(makeRow(10.0f, LayoutEdges(0.0f)));
            ui.addButton("OK");
            ui.addButton("Cancel");
            ui.addButton("Apply changes");
            ui.closeContainer();
        }
        ui.closeContainer();

        // 15 -- dividers take the full cross width of a column and only the thickness
        // they are given, and a thick one rounds into a capsule since the shader clamps
        // the radius to the shorter half-extent.
        caseBlock("15  dividers: 1 / 3 / 8 thick, the last one rounded");
        {
            LayoutConfig column = makeColumn(10.0f, LayoutEdges(8.0f));
            column.width = SizeSpec::fixed(360.0f);
            ui.openContainer(column, caseStyles);

            ui.addText("above the rule", labelStyle);
            dividerProbes.thin = ui.addDivider().index;
            ui.addText("between two rules", labelStyle);
            dividerProbes.thick = ui.addDivider(3.0f, UiPresets::divider(COLOR_CYAN)).index;
            ui.addText("below the rule", labelStyle);
            dividerProbes.rounded = ui.addDivider(8.0f, roundedDividerStyles).index;
            ui.closeContainer();
        }
        ui.closeContainer();

        ui.draw();
    };

    auto logLayoutRoot = [&]() {
        const std::vector<LayoutNode>& nodes = ui.getLayoutNodes();
        auto node = [&](uint index) -> const LayoutNode& { return nodes[index]; };

        LOG_INFO("--- root A probes ({} nodes) ---", nodes.size());
        LOG_INFO(
            "1  fit row width {} (expect 222 = 16 pad + 190 children + 16 gaps)",
            node(layoutProbes.fitRow).size.x
        );
        LOG_INFO(
            "2  growers {} and {} (expect equal, 154 each)", node(layoutProbes.growA).size.x,
            node(layoutProbes.growB).size.x
        );
        LOG_INFO(
            "3  capped growers {} and {} (expect 120 each), first x {} (expect 112)",
            node(layoutProbes.cappedA).size.x, node(layoutProbes.cappedB).size.x,
            node(layoutProbes.cappedA).pos.x
        );
        LOG_INFO(
            "4  fit parent {} = short {} + long {} + 16 pad + 8 gap (the two must differ)",
            node(layoutProbes.fitParent).size.x, node(layoutProbes.fitGrowShort).size.x,
            node(layoutProbes.fitGrowLong).size.x
        );
        LOG_INFO(
            "5  deepest box at depth {} is {}x{}", node(layoutProbes.deepInner).depth,
            node(layoutProbes.deepInner).size.x, node(layoutProbes.deepInner).size.y
        );
        for (int i = 0; i < 3; i++) {
            const LayoutNode& box = node(layoutProbes.alignBox[i]);
            const LayoutNode& child = node(layoutProbes.alignChild[i]);
            LOG_INFO(
                "6  align[{}] child inset ({}, {}) inside 160x70 (expect 6/6, 55/23, 104/40)", i,
                child.pos.x - box.pos.x, child.pos.y - box.pos.y
            );
        }
        LOG_INFO(
            "7  floating parent width {} (expect 222, same as case 1), floater at ({}, {})",
            node(layoutProbes.floatParent).size.x, node(layoutProbes.floatChild).pos.x,
            node(layoutProbes.floatChild).pos.y
        );
        LOG_INFO(
            "8  image {}x{} (expect height = width / 2)", node(layoutProbes.image).size.x,
            node(layoutProbes.image).size.y
        );
        LOG_INFO(
            "9  overflow row {} wide (spans x {} to {}), children spill to x {}",
            node(layoutProbes.overflowRow).size.x, node(layoutProbes.overflowRow).pos.x,
            node(layoutProbes.overflowRow).pos.x + node(layoutProbes.overflowRow).size.x,
            node(layoutProbes.overflowFirst).pos.x + 3.0f * 80.0f + 2.0f * 6.0f
        );
    };

    auto logTextRoot = [&]() {
        const std::vector<LayoutNode>& nodes = ui.getLayoutNodes();
        auto node = [&](uint index) -> const LayoutNode& { return nodes[index]; };

        auto logLines = [&](const char* label, uint index) {
            const TextLeaf* leaf = static_cast<const TextLeaf*>(ui.getElements()[index].measurer);
            std::string steps;
            for (const LayoutLine& line : leaf->getLines())
                steps += std::format("{:.1f} ", line.height);
            LOG_INFO(
                "{}: box {}x{}, {} lines, {} runs, steps [{}]", label, node(index).size.x,
                node(index).size.y, leaf->getLines().size(), leaf->getRuns().size(), steps
            );
        };

        LOG_INFO("--- root B probes ({} nodes) ---", nodes.size());
        LOG_INFO(
            "10 narrow column {}x{}, its text {}x{} over {} lines",
            node(textProbes.wrapNarrow).size.x, node(textProbes.wrapNarrow).size.y,
            node(textProbes.wrapText).size.x, node(textProbes.wrapText).size.y,
            node(textProbes.wrapText).lineCount
        );
        logLines("11 styled", textProbes.styledLines);
        logLines("12 styled+wrapped", textProbes.styledWrap);
        logLines("13 fixedLineHeight", textProbes.fixedLine);
        LOG_INFO(
            "15 dividers {}x{} / {}x{} / {}x{} (expect 344 wide, heights 1/3/8)",
            node(dividerProbes.thin).size.x, node(dividerProbes.thin).size.y,
            node(dividerProbes.thick).size.x, node(dividerProbes.thick).size.y,
            node(dividerProbes.rounded).size.x, node(dividerProbes.rounded).size.y
        );
    };

    auto onWindowUpdate = [&](IdType id, float dt) {
        int hAxis = Input::get().getAxis("Horizontal");
        int vAxis = Input::get().getAxis("Vertical");
        int zoomAxis = Input::get().getAxis("Zoom");

        // GLFW reports a phantom held key for the first frames after the window opens,
        // which would drift the fitted viewport before anything is touched.
        if (!inputSettled) {
            if (hAxis == 0 && vAxis == 0 && zoomAxis == 0) inputSettled = true;
        } else {
            Window* window = WindowManager::get().getWindow(id);
            Vec2 center(window->getWidth() * 0.5f, window->getHeight() * 0.5f);

            uiPan.x -= hAxis * dt * 600.0f;
            uiPan.y += vAxis * dt * 600.0f;

            // Zoom about the window centre, so the thing being looked at stays put
            // instead of sliding away from the top-left origin.
            if (zoomAxis != 0) {
                Vec2 anchor = (center - uiPan) / uiScale;
                uiScale = Math::max(0.15f, uiScale * (1.0f + zoomAxis * dt));
                uiPan = center - anchor * uiScale;
            }
        }

        if (Input::get().keyPressed(KeyCode::Tab)) {
            showBitmap = !showBitmap;
            ui.setDefaultFont(showBitmap ? &bitmapFont : &mtsdfFont);
            LOG_INFO("using the {} font", showBitmap ? "bitmap" : "mtsdf");
        }
        if (Input::get().keyPressed(KeyCode::Space)) {
            showText = !showText;
            UiRenderer::get().setDrawText(showText);
            LOG_INFO("ui text {}", showText ? "on" : "off");
        }
    };

    auto onWindowRender = [&](IdType id, float dt) {
        Renderer::get().beginPass();
        Renderer::get().setShader(&quadShader);
        Renderer::get().clearColor(Color(0.08f, 0.08f, 0.1f, 1.0f));

        // Fitted to the real client width rather than the requested one, since the
        // window manager can hand back something smaller (DPI scaling, for one).
        if (!viewportFitted) {
            Window* window = WindowManager::get().getWindow(id);
            uiScale = (window->getWidth() - 16.0f) / totalWidth;
            viewportFitted = true;
        }

        // Each root is its own begin/draw cycle, so the viewport moves between them
        // and the two trees never share a solve. Viewports are window pixels now, so
        // panning the UI is what explores the roots, not moving the camera.
        UiRenderer::get().setViewport(uiPan, uiScale);
        buildLayoutRoot();
        if (!logged) logLayoutRoot();

        // Recomputed every frame, since root A's width is draggable.
        Vec2 rootBPan(uiPan.x + (rootASize.x + ROOT_GAP) * uiScale, uiPan.y);
        UiRenderer::get().setViewport(rootBPan, uiScale);
        buildTextRoot();
        if (!logged) {
            logTextRoot();
            logged = true;
        }

        Renderer::get().beginPass();
        Renderer::get().setShader(&quadShader);
        Renderer::get().drawToWindow();
    };

    Application app(registry);
    app.onWindowUpdate().bind(&onWindowUpdate);
    app.onWindowRender().bind(&onWindowRender);
    app.run();

    UiSystem::get().shutdown();
    return 0;
}
