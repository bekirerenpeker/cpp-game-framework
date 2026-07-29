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
constexpr float ROOT_B_HEIGHT = 1180.0f;
constexpr float ROOT_GAP = 60.0f;
constexpr float WORLD_PER_UI = 0.01f;

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
    Renderer::get().init(6000, &quadShader);
    TextRenderer::get().init(&textShader, 8000);

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
    UiRenderer::get().setOutlineThickness(1.5f);

    const float totalWidth = ROOT_A_WIDTH + ROOT_GAP + ROOT_B_WIDTH;
    const float topWorldY = Math::max(ROOT_A_HEIGHT, ROOT_B_HEIGHT) * 0.5f * WORLD_PER_UI;
    const Vec2 rootAOrigin(-totalWidth * 0.5f * WORLD_PER_UI, topWorldY);
    const Vec2 rootBOrigin(rootAOrigin.x + (ROOT_A_WIDTH + ROOT_GAP) * WORLD_PER_UI, topWorldY);

    // Fitted to the pair's combined width; the roots are taller than the view, so
    // W/S pans down to the cases below the fold.
    const float fittedOrthoSize = totalWidth * WORLD_PER_UI * 1.05f;
    const float fittedY = topWorldY - fittedOrthoSize * 0.5f;
    camera.get<CameraComponent>().orthoSize = fittedOrthoSize;
    camera.get<TransformComponent>().position.y = fittedY;

    LayoutProbes layoutProbes;
    TextProbes textProbes;
    bool logged = false;
    bool inputSettled = false;
    bool showBitmap = false;
    bool showBoxes = true;

    auto caseBlock = [&](const char* title) {
        ui.openContainer(makeColumn(4.0f, LayoutEdges(0.0f)));
        ui.addText(title, labelStyle);
    };

    auto buildLayoutRoot = [&]() {
        ui.begin(Vec2(ROOT_A_WIDTH, ROOT_A_HEIGHT), makeColumn(14.0f, LayoutEdges(16.0f)));
        ui.addText("ROOT A - sizing, alignment, floating, overflow", titleStyle);

        // 1 -- a Fit row hugs its children plus padding and gaps.
        caseBlock("1  fit row: hugs 3 fixed children + padding + gaps");
        layoutProbes.fitRow = ui.openContainer(makeRow(8.0f, LayoutEdges(8.0f)));
        ui.openContainer(makeBox(60.0f, 30.0f));
        ui.closeContainer();
        ui.openContainer(makeBox(90.0f, 30.0f));
        ui.closeContainer();
        ui.openContainer(makeBox(40.0f, 30.0f));
        ui.closeContainer();
        ui.closeContainer();
        ui.closeContainer();

        // 2 -- surplus goes only to the growers, and they end up equal.
        caseBlock("2  fixed | grow | grow: the two growers end equal");
        {
            LayoutConfig config = makeRow(8.0f, LayoutEdges(8.0f));
            config.width = SizeSpec::fixed(440.0f);
            ui.openContainer(config);

            ui.openContainer(makeBox(100.0f, 30.0f));
            ui.closeContainer();

            LayoutConfig grower;
            grower.width = SizeSpec::grow();
            grower.height = SizeSpec::fixed(30.0f);
            layoutProbes.growA = ui.openContainer(grower);
            ui.closeContainer();
            layoutProbes.growB = ui.openContainer(grower);
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
            ui.openContainer(config);

            LayoutConfig capped;
            capped.width = SizeSpec::grow();
            capped.width.max = 120.0f;
            capped.height = SizeSpec::fixed(30.0f);
            layoutProbes.cappedA = ui.openContainer(capped);
            ui.closeContainer();
            layoutProbes.cappedB = ui.openContainer(capped);
            ui.closeContainer();
            ui.closeContainer();
        }
        ui.closeContainer();

        // 4 -- two growers in a Fit parent must keep their own content widths rather
        // than splitting evenly, or the long one wraps while the short one has slack.
        caseBlock("4  two growers in a fit parent: each keeps its own content width");
        {
            layoutProbes.fitParent = ui.openContainer(makeRow(8.0f, LayoutEdges(8.0f)));

            LayoutConfig grower;
            grower.width = SizeSpec::grow();
            layoutProbes.fitGrowShort = ui.addText("short", bodyStyle, grower);
            layoutProbes.fitGrowLong = ui.addText("a much longer label here", bodyStyle, grower);
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
                    uint first = ui.openContainer(makeBox(50.0f, 22.0f));
                    ui.closeContainer();
                    ui.openContainer(makeBox(70.0f, 22.0f));
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
                layoutProbes.alignBox[i] = ui.openContainer(config);
                layoutProbes.alignChild[i] = ui.openContainer(makeBox(50.0f, 24.0f));
                ui.closeContainer();
                ui.closeContainer();
            }
            ui.closeContainer();
        }
        ui.closeContainer();

        // 7 -- a floating child must not inflate its parent or consume a gap.
        caseBlock("7  floating child (red): parent width matches case 1's row exactly");
        {
            layoutProbes.floatParent = ui.openContainer(makeRow(8.0f, LayoutEdges(8.0f)));
            ui.openContainer(makeBox(60.0f, 30.0f));
            ui.closeContainer();
            ui.openContainer(makeBox(90.0f, 30.0f));
            ui.closeContainer();
            ui.openContainer(makeBox(40.0f, 30.0f));
            ui.closeContainer();

            LayoutConfig floater = makeBox(70.0f, 20.0f);
            floater.isFloating = true;
            floater.floating.anchorX = LayoutAlign::End;
            floater.floating.anchorY = LayoutAlign::End;
            floater.floating.selfX = LayoutAlign::Center;
            floater.floating.selfY = LayoutAlign::Center;
            layoutProbes.floatChild = ui.openContainer(floater);
            ui.closeContainer();
            ui.closeContainer();
        }
        ui.closeContainer();

        // 8 -- a non-text leaf whose height also depends on its final width.
        caseBlock("8  image, aspect 2:1, grow width: height tracks the final width");
        {
            LayoutConfig config = makeRow(0.0f, LayoutEdges(6.0f));
            config.width = SizeSpec::fixed(380.0f);
            ui.openContainer(config);

            LayoutConfig imageConfig;
            imageConfig.width = SizeSpec::grow();
            layoutProbes.image =
                ui.addImage(mtsdfFont.getTexture(), Vec2(200.0f, 100.0f), 2.0f, imageConfig);
            ui.closeContainer();
        }
        ui.closeContainer();

        // 9 -- every child at its minimum and still not fitting is real overflow.
        caseBlock("9  overflow: three 80-wide children in a 150-wide row");
        {
            LayoutConfig config = makeRow(6.0f, LayoutEdges(6.0f));
            config.width = SizeSpec::fixed(150.0f);
            layoutProbes.overflowRow = ui.openContainer(config);
            layoutProbes.overflowFirst = ui.openContainer(makeBox(80.0f, 26.0f));
            ui.closeContainer();
            ui.openContainer(makeBox(80.0f, 26.0f));
            ui.closeContainer();
            ui.openContainer(makeBox(80.0f, 26.0f));
            ui.closeContainer();
            ui.closeContainer();
        }
        ui.closeContainer();

        ui.draw();
    };

    auto buildTextRoot = [&]() {
        ui.begin(Vec2(ROOT_B_WIDTH, ROOT_B_HEIGHT), makeColumn(14.0f, LayoutEdges(16.0f)));
        ui.addText("ROOT B - wrapping, styled spans, line height", titleStyle);

        // 10 -- shrinking is what drives the wrap, and the box grows taller for it.
        caseBlock("10  wrap: same text at 230 / 150 / 95 wide");
        {
            ui.openContainer(makeRow(12.0f, LayoutEdges(0.0f)));
            const char* sample = "The quick brown fox jumps over the lazy dog near the riverbank";
            const float widths[] = {230.0f, 150.0f, 95.0f};
            for (int i = 0; i < 3; i++) {
                LayoutConfig column = makeColumn(0.0f, LayoutEdges(6.0f));
                column.width = SizeSpec::fixed(widths[i]);
                uint index = ui.openContainer(column);

                LayoutConfig textConfig;
                textConfig.width = SizeSpec::grow();
                uint textIndex = ui.addText(sample, bodyStyle, textConfig);
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
                "first line is plain\nsecond has a /sBIG/s word in it\nthird /stiny/s and plain\n"
                "fourth line is plain again",
                bodyStyle, mixedStyles, textConfig
            );
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
                uint index = ui.addText(
                    "the quick /sbrown/s fox jumps over the /slazy/s dog and keeps running "
                    "along the riverbank",
                    bodyStyle, mixedStyles, textConfig
                );
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
                "first line is plain\nsecond has a /sBIG/s word in it\nthird /stiny/s and plain\n"
                "fourth line is plain again",
                bodyStyle, mixedStyles, textConfig, true, true
            );
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
    };

    auto onWindowUpdate = [&](IdType id, float dt) {
        TransformComponent& transform = camera.get<TransformComponent>();
        CameraComponent& cam = camera.get<CameraComponent>();

        int hAxis = Input::get().getAxis("Horizontal");
        int vAxis = Input::get().getAxis("Vertical");
        int zoomAxis = Input::get().getAxis("Zoom");

        // GLFW reports a phantom held key for the first frames after the window
        // opens, which would drift the fitted camera before anything is touched.
        if (!inputSettled) {
            if (hAxis == 0 && vAxis == 0 && zoomAxis == 0) inputSettled = true;
            cam.orthoSize = fittedOrthoSize;
            transform.position.x = 0.0f;
            transform.position.y = fittedY;
        } else {
            transform.position.x += hAxis * dt * cam.orthoSize;
            transform.position.y += vAxis * dt * cam.orthoSize;
            cam.orthoSize -= zoomAxis * dt * cam.orthoSize;
        }

        if (Input::get().keyPressed(KeyCode::Tab)) {
            showBitmap = !showBitmap;
            ui.setDefaultFont(showBitmap ? &bitmapFont : &mtsdfFont);
            LOG_INFO("using the {} font", showBitmap ? "bitmap" : "mtsdf");
        }
        if (Input::get().keyPressed(KeyCode::Space)) {
            showBoxes = !showBoxes;
            UiRenderer::get().setDrawBoxes(showBoxes);
            LOG_INFO("debug boxes {}", showBoxes ? "on" : "off");
        }
    };

    auto onWindowRender = [&](IdType id, float dt) {
        Renderer::get().beginPass();
        Renderer::get().setShader(&quadShader);
        Renderer::get().clearColor(Color(0.08f, 0.08f, 0.1f, 1.0f));

        // Each root is its own begin/draw cycle, so the viewport moves between them
        // and the two trees never share a solve.
        UiRenderer::get().setViewport(rootAOrigin, WORLD_PER_UI);
        buildLayoutRoot();
        if (!logged) logLayoutRoot();

        UiRenderer::get().setViewport(rootBOrigin, WORLD_PER_UI);
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
