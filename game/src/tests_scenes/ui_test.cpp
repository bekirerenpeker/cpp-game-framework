#include "EngineInclude.hpp"
#include "test_funcs.hpp"
#include "utils/math/MathFuncs.hpp"

using namespace Engine;

namespace {

const char* FONT_FOLDER = "game/assets/fonts";
const char* FALLBACK_FONT = "C:/Windows/Fonts/segoeui.ttf";

const Vec2 ROOT_SIZE(900.0f, 600.0f);

fs::path findFontFile()
{
    if (FileManager::get().doesPathExist(FONT_FOLDER)) {
        for (const auto& entry : fs::directory_iterator(FONT_FOLDER)) {
            std::string ext = entry.path().extension().string();
            if (ext == ".ttf" || ext == ".otf") return entry.path();
        }
    }
    return FALLBACK_FONT;
}

UIContainerStyle fill(Color color)
{
    UIContainerStyle style;
    style.backgroundColor = color;
    return style;
}

UILayoutConfig box(UISizeSpec width, UISizeSpec height)
{
    UILayoutConfig config;
    config.width = width;
    config.height = height;
    return config;
}

// UIManager has no leaf builder yet, so the leaf is attached by hand and its
// lifetime stays with the caller -- nothing in the tree owns it.
IdType addText(
    IdType parent, std::vector<IUILeafData*>& owned, const Font& font, std::string_view text,
    float size, Color color
)
{
    UILayoutConfig layout = box(UISizeSpec::grow(), UISizeSpec::fit());

    IdType id = UIManager::get().addContainer(parent, layout);
    TextStyle style {.color = color, .size = size};
    UITextLeafData* leaf = new UITextLeafData(&font, text, style);

    UIManager::get().getNode(id)->leafData = leaf;
    owned.push_back(leaf);
    return id;
}

}   // namespace

// Exercises the retained UI tree end to end: UIManager builds the nodes,
// UILayoutCalculator solves them, and UINode's debug quad draws the result.
// Layout is Y-down while the camera is Y-up, so the tree renders vertically
// mirrored until a real UI renderer owns that flip.
// WASD pans, QE zooms.
int ui_test()
{
    IdType windowId = WindowManager::get().createWindow({1000, 800, "UI Test"});

    Registry registry;
    EntityHandle camera = registry.create();
    camera.emplace<TransformComponent>().position = Vec3(ROOT_SIZE.x * 0.5f, ROOT_SIZE.y * 0.5f, 0);
    camera.emplace<CameraComponent>().windowId = windowId;
    camera.get<CameraComponent>().orthoSize = ROOT_SIZE.y * 1.15f;

    Font font(findFontFile(), {.atlasType = FontAtlasType::Mtsdf, .emPixelSize = 48});

    GlShader quadShader("game/assets/shaders/QuadShader.glsl");
    GlShader textShader("game/assets/shaders/TextShader.glsl");
    Renderer::get().init(2000, &quadShader);
    TextRenderer::get().init(&textShader);

    Input::get().addAxis("Horizontal", {KeyCode::D, KeyCode::A});
    Input::get().addAxis("Vertical", {KeyCode::W, KeyCode::S});
    Input::get().addAxis("Zoom", {KeyCode::E, KeyCode::Q});

    UIManager& ui = UIManager::get();
    std::vector<IUILeafData*> leaves;

    UILayoutConfig rootLayout =
        box(UISizeSpec::fixed(ROOT_SIZE.x + Math::sin(Time::get().currTime()) * 50.0f),
            UISizeSpec::fixed(ROOT_SIZE.y));
    rootLayout.direction = UILayoutDirection::Column;
    rootLayout.padding = UIEdges(20.0f);
    rootLayout.gap = 16.0f;
    IdType root = ui.addContainer(INVALID_ID, rootLayout, fill(Color(0.10f, 0.11f, 0.14f)));

    UILayoutConfig headerLayout = box(UISizeSpec::grow(), UISizeSpec::fixed(64.0f));
    headerLayout.padding = UIEdges(16.0f, 12.0f);
    headerLayout.alignCross = UIAlign::Center;
    IdType header = ui.addContainer(root, headerLayout, fill(Color(0.20f, 0.23f, 0.30f)));
    addText(header, leaves, font, "UI System Test", 28.0f, COLOR_WHITE);

    UILayoutConfig bodyLayout = box(UISizeSpec::grow(), UISizeSpec::grow());
    bodyLayout.gap = 16.0f;
    IdType body = ui.addContainer(root, bodyLayout);

    // Fixed-width sidebar against a growing content pane: the two size modes have
    // to share one row without either collapsing.
    UILayoutConfig sidebarLayout = box(UISizeSpec::fixed(220.0f), UISizeSpec::grow());
    sidebarLayout.direction = UILayoutDirection::Column;
    sidebarLayout.padding = UIEdges(12.0f);
    sidebarLayout.gap = 8.0f;
    IdType sidebar = ui.addContainer(body, sidebarLayout, fill(Color(0.14f, 0.16f, 0.20f)));
    for (int i = 0; i < 3; i++) {
        ui.addContainer(
            sidebar, box(UISizeSpec::grow(), UISizeSpec::fixed(36.0f)),
            fill(Color(0.24f, 0.27f, 0.34f))
        );
    }

    UILayoutConfig contentLayout = box(UISizeSpec::grow(), UISizeSpec::grow());
    contentLayout.direction = UILayoutDirection::Column;
    contentLayout.padding = UIEdges(16.0f);
    contentLayout.gap = 12.0f;
    IdType content = ui.addContainer(body, contentLayout, fill(Color(0.14f, 0.16f, 0.20f)));
    addText(content, leaves, font, "Content", 24.0f, COLOR_CYAN);
    addText(
        content, leaves, font,
        "This paragraph is long enough that it has to wrap inside the content pane, "
        "which is what proves the text leaf's height follows its solved width.",
        18.0f, Color(0.78f, 0.80f, 0.86f)
    );

    UILayoutConfig footerLayout = box(UISizeSpec::grow(), UISizeSpec::fixed(40.0f));
    footerLayout.padding = UIEdges(12.0f, 8.0f);
    footerLayout.alignCross = UIAlign::Center;
    IdType footer = ui.addContainer(root, footerLayout, fill(Color(0.20f, 0.23f, 0.30f)));
    addText(footer, leaves, font, "footer", 16.0f, Color(0.70f, 0.73f, 0.80f));

    auto onWindowUpdate = [&](IdType id, float dt) {
        TransformComponent& transform = camera.get<TransformComponent>();
        CameraComponent& cam = camera.get<CameraComponent>();

        transform.position.x += Input::get().getAxis("Horizontal") * dt * cam.orthoSize;
        transform.position.y += Input::get().getAxis("Vertical") * dt * cam.orthoSize;
        cam.orthoSize -= Input::get().getAxis("Zoom") * dt * cam.orthoSize;
    };

    auto onWindowRender = [&](IdType id, float dt) {
        Renderer::get().beginPass();
        Renderer::get().setShader(&quadShader);
        Renderer::get().clearColor(Color(0.05f, 0.05f, 0.07f, 1.0f));

        UIManager::get().draw(root, ROOT_SIZE);

        // Boxes and glyphs are separate batches, so the quads have to resolve
        // before any glyph is submitted or the two draw in whichever order.
        Renderer::get().endScene();
        TextRenderer::get().flush();

        Renderer::get().beginPass();
        Renderer::get().setShader(&quadShader);
        Renderer::get().drawToWindow();
    };

    Application app(registry);
    app.onWindowUpdate().bind(&onWindowUpdate);
    app.onWindowRender().bind(&onWindowRender);
    app.run();

    for (IUILeafData* leaf : leaves) delete leaf;
    return 0;
}
