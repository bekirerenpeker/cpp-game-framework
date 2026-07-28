#include "EngineInclude.hpp"
#include "test_funcs.hpp"

using namespace Engine;

namespace {

const char* FONT_FOLDER = "game/assets/fonts";
const char* FALLBACK_FONT = "C:/Windows/Fonts/segoeui.ttf";

// Layout is unitless; this scene treats one UI unit as one pixel of a 1000-wide
// design and scales the whole thing into world space for the debug boxes.
constexpr float ROOT_WIDTH = 1000.0f;
constexpr float ROOT_HEIGHT = 1560.0f;
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

// Every case index that gets checked numerically rather than only by eye.
struct Probes
{
    uint fitRow = NO_NODE;
    uint growA = NO_NODE;
    uint growB = NO_NODE;
    uint cappedA = NO_NODE;
    uint cappedB = NO_NODE;
    uint fitParent = NO_NODE;
    uint fitGrowShort = NO_NODE;
    uint fitGrowLong = NO_NODE;
    uint wrapNarrow = NO_NODE;
    uint wrapText = NO_NODE;
    uint floatParent = NO_NODE;
    uint floatChild = NO_NODE;
    uint image = NO_NODE;
    uint overflowRow = NO_NODE;
    uint overflowFirst = NO_NODE;
    uint deepInner = NO_NODE;
    uint alignBox[3] = {NO_NODE, NO_NODE, NO_NODE};
    uint alignChild[3] = {NO_NODE, NO_NODE, NO_NODE};
};

}   // namespace

// Exercises the five-pass layout solver and draws every computed box. Boxes are
// tinted by tree depth, floating nodes are red. WASD pans, Q/E zooms -- the box
// tree must be identical at every zoom, since layout is unitless.
int ui_layout_test()
{
    IdType windowId = WindowManager::get().createWindow({900, 900, "UI Layout Test"});

    Registry registry;
    EntityHandle camera = registry.create();
    camera.emplace<TransformComponent>();
    camera.emplace<CameraComponent>().windowId = windowId;

    Font font(findFontFile(), {.atlasType = FontAtlasType::Mtsdf, .emPixelSize = 48});
    if (!font.isValid()) {
        LOG_ERROR("no usable font; text leaves would all measure as empty");
        return 1;
    }

    GlShader quadShader("game/assets/shaders/QuadShader.glsl");
    Renderer::get().init(6000, &quadShader);

    Input::get().addAxis("Horizontal", {KeyCode::D, KeyCode::A});
    Input::get().addAxis("Vertical", {KeyCode::W, KeyCode::S});
    Input::get().addAxis("Zoom", {KeyCode::E, KeyCode::Q});

    const TextStyle bodyStyle {.color = COLOR_WHITE, .size = 22.0f};
    const TextStyle labelStyle {.color = COLOR_GRAY, .size = 18.0f};

    UiSystem& ui = UiSystem::get();
    ui.setDefaultFont(&font);
    ui.setDefaultTextStyle(bodyStyle);
    ui.setDebugViewport(
        Vec2(-ROOT_WIDTH * 0.5f * WORLD_PER_UI, ROOT_HEIGHT * 0.5f * WORLD_PER_UI), WORLD_PER_UI
    );
    ui.getDebugDrawer().setOutlineThickness(2.0f);

    const float fittedOrthoSize = ROOT_HEIGHT * WORLD_PER_UI * 1.04f;
    camera.get<CameraComponent>().orthoSize = fittedOrthoSize;

    Probes probes;
    bool logged = false;
    bool inputSettled = false;

    auto buildUi = [&]() {
        LayoutConfig rootConfig = makeColumn(16.0f, LayoutEdges(16.0f));
        ui.begin(Vec2(ROOT_WIDTH, ROOT_HEIGHT), rootConfig);

        auto caseBlock = [&](const char* title) {
            ui.openContainer(makeColumn(4.0f, LayoutEdges(0.0f)));
            ui.addText(title, labelStyle);
        };

        // 1 -- a Fit row hugs its children plus padding and gaps.
        caseBlock("1  fit row: hugs 3 fixed children + padding + gaps");
        probes.fitRow = ui.openContainer(makeRow(8.0f, LayoutEdges(8.0f)));
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
            config.width = SizeSpec::fixed(600.0f);
            ui.openContainer(config);

            LayoutConfig fixedChild = makeBox(100.0f, 30.0f);
            ui.openContainer(fixedChild);
            ui.closeContainer();

            LayoutConfig grower;
            grower.width = SizeSpec::grow();
            grower.height = SizeSpec::fixed(30.0f);
            probes.growA = ui.openContainer(grower);
            ui.closeContainer();
            probes.growB = ui.openContainer(grower);
            ui.closeContainer();
            ui.closeContainer();
        }
        ui.closeContainer();

        // 3 -- a grower is still bounded by its own max; the leftover is alignMain's.
        caseBlock("3  grow capped by sizing.max: both stop at 120, leftover centred");
        {
            LayoutConfig config = makeRow(8.0f, LayoutEdges(8.0f));
            config.width = SizeSpec::fixed(600.0f);
            config.alignMain = LayoutAlign::Center;
            ui.openContainer(config);

            LayoutConfig capped;
            capped.width = SizeSpec::grow();
            capped.width.max = 120.0f;
            capped.height = SizeSpec::fixed(30.0f);
            probes.cappedA = ui.openContainer(capped);
            ui.closeContainer();
            probes.cappedB = ui.openContainer(capped);
            ui.closeContainer();
            ui.closeContainer();
        }
        ui.closeContainer();

        // 4 -- two growers in a Fit parent must keep their own content widths rather
        // than splitting evenly, or the long one wraps while the short one has slack.
        caseBlock("4  two growers in a fit parent: each keeps its own content width");
        {
            LayoutConfig config = makeRow(8.0f, LayoutEdges(8.0f));
            probes.fitParent = ui.openContainer(config);

            LayoutConfig grower;
            grower.width = SizeSpec::grow();
            probes.fitGrowShort = ui.addText("short", bodyStyle, grower);
            probes.fitGrowLong = ui.addText("a considerably longer label here", bodyStyle, grower);
            ui.closeContainer();
        }
        ui.closeContainer();

        // 5 -- shrinking is what drives the wrap, and the box grows taller for it.
        caseBlock("5  wrap: same text at 460 / 260 / 120 wide");
        {
            ui.openContainer(makeRow(12.0f, LayoutEdges(0.0f)));
            const char* sample = "The quick brown fox jumps over the lazy dog near the riverbank";
            const float widths[] = {460.0f, 260.0f, 120.0f};
            for (int i = 0; i < 3; i++) {
                LayoutConfig column = makeColumn(0.0f, LayoutEdges(6.0f));
                column.width = SizeSpec::fixed(widths[i]);
                uint index = ui.openContainer(column);

                LayoutConfig textConfig;
                textConfig.width = SizeSpec::grow();
                uint textIndex = ui.addText(sample, bodyStyle, textConfig);
                ui.closeContainer();

                if (i == 2) {
                    probes.wrapNarrow = index;
                    probes.wrapText = textIndex;
                }
            }
            ui.closeContainer();
        }
        ui.closeContainer();

        // 6 -- nesting three levels deep, alternating direction.
        caseBlock("6  nested row > column > row, 3 levels");
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
                    if (outer == 0 && inner == 0) probes.deepInner = first;
                }
                ui.closeContainer();
            }
            ui.closeContainer();
        }
        ui.closeContainer();

        // 7 -- main-axis and cross-axis alignment.
        caseBlock("7  alignment: main start/centre/end, cross start/centre/end");
        {
            ui.openContainer(makeRow(12.0f, LayoutEdges(0.0f)));
            const LayoutAlign aligns[] = {
                LayoutAlign::Start, LayoutAlign::Center, LayoutAlign::End
            };
            for (int i = 0; i < 3; i++) {
                LayoutConfig config = makeRow(0.0f, LayoutEdges(6.0f));
                config.width = SizeSpec::fixed(180.0f);
                config.height = SizeSpec::fixed(70.0f);
                config.alignMain = aligns[i];
                config.alignCross = aligns[i];
                probes.alignBox[i] = ui.openContainer(config);
                probes.alignChild[i] = ui.openContainer(makeBox(50.0f, 24.0f));
                ui.closeContainer();
                ui.closeContainer();
            }
            ui.closeContainer();
        }
        ui.closeContainer();

        // 8 -- a floating child must not inflate its parent or consume a gap.
        caseBlock("8  floating child (red): parent width matches case 1's row exactly");
        {
            probes.floatParent = ui.openContainer(makeRow(8.0f, LayoutEdges(8.0f)));
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
            probes.floatChild = ui.openContainer(floater);
            ui.closeContainer();
            ui.closeContainer();
        }
        ui.closeContainer();

        // 9 -- a non-text leaf whose height also depends on its final width.
        caseBlock("9  image, aspect 2:1, grow width: height tracks the final width");
        {
            LayoutConfig config = makeRow(0.0f, LayoutEdges(6.0f));
            config.width = SizeSpec::fixed(400.0f);
            ui.openContainer(config);

            LayoutConfig imageConfig;
            imageConfig.width = SizeSpec::grow();
            probes.image = ui.addImage(font.getTexture(), Vec2(200.0f, 100.0f), 2.0f, imageConfig);
            ui.closeContainer();
        }
        ui.closeContainer();

        // 10 -- every child at its minimum and still not fitting is real overflow.
        caseBlock("10  overflow: three 80-wide children in a 150-wide row");
        {
            LayoutConfig config = makeRow(6.0f, LayoutEdges(6.0f));
            config.width = SizeSpec::fixed(150.0f);
            probes.overflowRow = ui.openContainer(config);
            probes.overflowFirst = ui.openContainer(makeBox(80.0f, 26.0f));
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

    auto logResults = [&]() {
        const std::vector<LayoutNode>& nodes = ui.getLayoutNodes();
        auto node = [&](uint index) -> const LayoutNode& { return nodes[index]; };

        LOG_INFO("--- layout probes ({} nodes) ---", nodes.size());
        LOG_INFO(
            "1  fit row width {} (expect 222 = 16 pad + 190 children + 16 gaps)",
            node(probes.fitRow).size.x
        );
        LOG_INFO(
            "2  growers {} and {} (expect equal, 234 each)", node(probes.growA).size.x,
            node(probes.growB).size.x
        );
        LOG_INFO(
            "3  capped growers {} and {} (expect 120 each), first x {}",
            node(probes.cappedA).size.x, node(probes.cappedB).size.x, node(probes.cappedA).pos.x
        );
        LOG_INFO(
            "4  fit parent {} = short {} + long {} + 16 pad + 8 gap (the two must differ)",
            node(probes.fitParent).size.x, node(probes.fitGrowShort).size.x,
            node(probes.fitGrowLong).size.x
        );
        LOG_INFO(
            "5  narrow column {}x{}, its text {}x{} over {} lines", node(probes.wrapNarrow).size.x,
            node(probes.wrapNarrow).size.y, node(probes.wrapText).size.x,
            node(probes.wrapText).size.y, node(probes.wrapText).lineCount
        );
        LOG_INFO(
            "8  floating parent width {} (expect 222, same as case 1), floater at ({}, {})",
            node(probes.floatParent).size.x, node(probes.floatChild).pos.x,
            node(probes.floatChild).pos.y
        );
        LOG_INFO(
            "9  image {}x{} (expect height = width / 2)", node(probes.image).size.x,
            node(probes.image).size.y
        );
        LOG_INFO(
            "10 overflow row {} wide (spans x {} to {}), children spill to x {}",
            node(probes.overflowRow).size.x, node(probes.overflowRow).pos.x,
            node(probes.overflowRow).pos.x + node(probes.overflowRow).size.x,
            node(probes.overflowFirst).pos.x + 3.0f * 80.0f + 2.0f * 6.0f
        );
        LOG_INFO(
            "6  deepest box at depth {} is {}x{} at ({}, {})", node(probes.deepInner).depth,
            node(probes.deepInner).size.x, node(probes.deepInner).size.y,
            node(probes.deepInner).pos.x, node(probes.deepInner).pos.y
        );
        for (int i = 0; i < 3; i++) {
            const LayoutNode& box = node(probes.alignBox[i]);
            const LayoutNode& child = node(probes.alignChild[i]);
            LOG_INFO(
                "7  align[{}] child inset ({}, {}) inside 180x70 (expect 6/6, 65/23, 124/40)", i,
                child.pos.x - box.pos.x, child.pos.y - box.pos.y
            );
        }
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
            transform.position.y = 0.0f;
        } else {
            transform.position.x += hAxis * dt * cam.orthoSize;
            transform.position.y += vAxis * dt * cam.orthoSize;
            cam.orthoSize -= zoomAxis * dt * cam.orthoSize;
        }
    };

    auto onWindowRender = [&](IdType id, float dt) {
        Renderer::get().beginPass();
        Renderer::get().setShader(&quadShader);
        Renderer::get().clearColor(Color(0.08f, 0.08f, 0.1f, 1.0f));

        buildUi();

        if (!logged) {
            logResults();
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
