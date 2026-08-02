#include "EngineInclude.hpp"
#include "test_funcs.hpp"

using namespace Engine;

namespace {

const char* FONT_FOLDER = "game/assets/fonts";
const char* FALLBACK_FONT = "C:/Windows/Fonts/segoeui.ttf";
const char* IMAGE_PATH = "game/assets/images/mario.png";

const Vec2 ROOT_SIZE(980.0f, 560.0f);
const Vec2 CELL_SIZE(96.0f, 64.0f);

const Color BACKDROP(0.05f, 0.05f, 0.07f, 1.0f);
const Color SURFACE(0.13f, 0.14f, 0.18f);
const Color PANEL(0.17f, 0.18f, 0.23f);
const Color SWATCH(0.24f, 0.26f, 0.33f);
const Color ACCENT(0.30f, 0.62f, 0.95f);
const Color OUTLINE(0.28f, 0.31f, 0.40f);
const Color TITLE_COLOR(0.90f, 0.92f, 0.96f);
const Color LABEL_COLOR(0.82f, 0.85f, 0.91f);
const Color CAPTION_COLOR(0.56f, 0.59f, 0.68f);

// 0..1, so a style field can be written as base + pulse * range.
float pulse(float speed) { return 0.5f + 0.5f * Math::sin(Time::get().currTime() * speed); }

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

UILayoutConfig box(UISizeSpec width, UISizeSpec height)
{
    UILayoutConfig config;
    config.width = width;
    config.height = height;
    return config;
}

UILayoutConfig textBox() { return box(UISizeSpec::grow(), UISizeSpec::fit()); }

void label(const Font& font, const char* text, Color color, float size)
{
    UIManager::get().addTextLeaf(
        textBox(), {
                       .font = &font, .text = text, .style = {.color = color, .size = size}
    }
    );
}

// A swatch of the style under test with its name underneath. The swatch's state comes
// back so a caller can rewrite the style it was just given -- nothing reads a style
// until draw(), so a hover landing here still lands in time.
UINodeState cell(const Font& font, const UIContainerStyle& style, const char* caption)
{
    UIManager& ui = UIManager::get();

    UILayoutConfig column = box(UISizeSpec::fixed(CELL_SIZE.x), UISizeSpec::fit());
    column.direction = UILayoutDirection::Column;
    column.gap = 6.0f;

    ui.openContainer(column);
    UINodeState state =
        ui.openContainer(box(UISizeSpec::grow(), UISizeSpec::fixed(CELL_SIZE.y)), style);
    ui.closeContainer();

    ui.addTextLeaf(
        textBox(), {
                       .font = &font,
                       .text = caption,
                       .style = {.color = CAPTION_COLOR, .size = 12.0f},
                       .alignment = {.horizontal = TextAlignH::Center}
    }
    );
    ui.closeContainer();

    return state;
}

// A titled row of swatches; the caller puts cell()s between the two calls.
void beginSection(const Font& font, const char* title)
{
    UIManager& ui = UIManager::get();

    UILayoutConfig column = box(UISizeSpec::grow(), UISizeSpec::fit());
    column.direction = UILayoutDirection::Column;
    column.gap = 8.0f;

    ui.openContainer(column);
    label(font, title, LABEL_COLOR, 16.0f);

    UILayoutConfig row = box(UISizeSpec::grow(), UISizeSpec::fit());
    row.gap = 12.0f;
    ui.openContainer(row);
}

void endSection()
{
    UIManager::get().closeContainer();
    UIManager::get().closeContainer();
}

void beginPanel()
{
    UILayoutConfig layout = box(UISizeSpec::grow(), UISizeSpec::grow());
    layout.direction = UILayoutDirection::Column;
    layout.padding = UIEdges(16.0f);
    layout.gap = 16.0f;

    UIManager::get().openContainer(layout, {.backgroundColor = PANEL, .borderRadius = 12.0f});
}

void buildBackgroundSection(const Font& font, GlTexture& image)
{
    beginSection(font, "Background");
    cell(font, {.backgroundColor = ACCENT}, "solid");
    cell(font, {.backgroundColor = Color(0.30f, 0.62f, 0.95f, 0.35f)}, "alpha 0.35");
    cell(font, {.backgroundImage = &image}, "image");
    cell(font, {.backgroundColor = Color(1.0f, 0.55f, 0.35f), .backgroundImage = &image}, "tinted");
    endSection();
}

void buildRadiusSection(const Font& font)
{
    beginSection(font, "Corner radius");
    cell(font, {.backgroundColor = SWATCH, .borderRadius = 0.0f}, "0");
    cell(font, {.backgroundColor = SWATCH, .borderRadius = 10.0f}, "10");
    cell(font, {.backgroundColor = SWATCH, .borderRadius = 26.0f}, "26");
    // Clamped to half the shorter side, so an absurd radius is a capsule, not a
    // distance field folded inside out.
    cell(font, {.backgroundColor = SWATCH, .borderRadius = 999.0f}, "clamped");
    endSection();
}

void buildBorderSection(const Font& font)
{
    beginSection(font, "Border width");
    cell(
        font,
        {.backgroundColor = SWATCH,
         .borderColor = ACCENT,
         .borderWidth = 2.0f,
         .borderRadius = 8.0f},
        "2"
    );
    cell(
        font,
        {.backgroundColor = SWATCH,
         .borderColor = ACCENT,
         .borderWidth = 6.0f,
         .borderRadius = 8.0f},
        "6"
    );
    cell(
        font,
        {.backgroundColor = SWATCH,
         .borderColor = ACCENT,
         .borderWidth = 14.0f,
         .borderRadius = 8.0f},
        "14"
    );
    // No background at all: the ring is the whole element, and the panel behind it
    // shows through the middle.
    cell(font, {.borderColor = ACCENT, .borderWidth = 3.0f, .borderRadius = 8.0f}, "no fill");
    endSection();
}

void buildBorderStyleSection(const Font& font)
{
    beginSection(font, "Border style");
    cell(
        font,
        {.backgroundColor = SWATCH,
         .borderColor = ACCENT,
         .borderWidth = 4.0f,
         .borderRadius = 8.0f,
         .borderStyle = UIBorderStyle::Solid},
        "solid"
    );
    cell(
        font,
        {.backgroundColor = SWATCH,
         .borderColor = ACCENT,
         .borderWidth = 4.0f,
         .borderRadius = 8.0f,
         .borderStyle = UIBorderStyle::Dashed},
        "dashed"
    );
    cell(
        font,
        {.backgroundColor = SWATCH,
         .borderColor = ACCENT,
         .borderWidth = 4.0f,
         .borderRadius = 8.0f,
         .borderStyle = UIBorderStyle::Dotted},
        "dotted"
    );
    // The dash walk is arc length around the rounded boundary, so the pattern keeps
    // its spacing through the corners instead of bunching up.
    cell(
        font,
        {.backgroundColor = SWATCH,
         .borderColor = ACCENT,
         .borderWidth = 4.0f,
         .borderRadius = 30.0f,
         .borderStyle = UIBorderStyle::Dashed},
        "dashed round"
    );
    endSection();
}

void buildShadowSection(const Font& font)
{
    beginSection(font, "Shadow");
    cell(
        font,
        {.backgroundColor = SWATCH,
         .borderRadius = 10.0f,
         .shadowColor = Color(0.0f, 0.0f, 0.0f, 0.8f),
         .shadowOffset = Vec2(6.0f, 8.0f),
         .shadowBlurRadius = 2.0f},
        "drop"
    );
    cell(
        font,
        {.backgroundColor = SWATCH,
         .borderRadius = 10.0f,
         .shadowColor = Color(0.0f, 0.0f, 0.0f, 0.85f),
         .shadowOffset = Vec2(0.0f, 6.0f),
         .shadowBlurRadius = 16.0f},
        "blurred"
    );
    // A zero offset makes the shadow a centred glow rather than a drop.
    cell(
        font,
        {.backgroundColor = SWATCH,
         .borderRadius = 10.0f,
         .shadowColor = ACCENT,
         .shadowBlurRadius = 18.0f},
        "glow"
    );
    // The shadow is knocked out under the box the way CSS does it, so this barely
    // tinted fill shows the panel behind it rather than its own shadow.
    cell(
        font,
        {.backgroundColor = Color(1.0f, 1.0f, 1.0f, 0.12f),
         .borderRadius = 10.0f,
         .shadowColor = Color(0.0f, 0.0f, 0.0f, 0.85f),
         .shadowBlurRadius = 14.0f},
        "knockout"
    );
    endSection();
}

void buildLiveSection(const Font& font)
{
    beginSection(font, "Per frame");
    cell(font, {.backgroundColor = SWATCH, .borderRadius = 2.0f + pulse(1.3f) * 30.0f}, "radius");
    cell(
        font,
        {.backgroundColor = SWATCH,
         .borderColor = ACCENT,
         .borderWidth = 1.0f + pulse(1.7f) * 13.0f,
         .borderRadius = 8.0f},
        "width"
    );
    cell(
        font,
        {.backgroundColor = SWATCH,
         .borderRadius = 10.0f,
         .shadowColor = ACCENT,
         .shadowBlurRadius = 2.0f + pulse(0.9f) * 26.0f},
        "blur"
    );

    UINodeState state = cell(font, {.backgroundColor = SWATCH, .borderRadius = 10.0f}, "hover me");
    if (state.isHovered) {
        UINode* node = UIManager::get().getNode(state.id);
        node->style.backgroundColor = ACCENT;
        node->style.borderColor = COLOR_WHITE;
        node->style.borderWidth = 3.0f;
        node->style.shadowColor = Color(ACCENT.r, ACCENT.g, ACCENT.b, 0.7f);
        node->style.shadowBlurRadius = 20.0f;
    }
    endSection();
}

// Declared from scratch every frame. Nesting is the open/close pairing, so no id is
// ever handled and the root is simply the container opened with nothing else open.
void buildUi(const Font& font, GlTexture& image)
{
    UIManager& ui = UIManager::get();
    ui.clear();

    UILayoutConfig rootLayout = box(UISizeSpec::fixed(ROOT_SIZE.x), UISizeSpec::fixed(ROOT_SIZE.y));
    rootLayout.direction = UILayoutDirection::Column;
    rootLayout.padding = UIEdges(24.0f);
    rootLayout.gap = 16.0f;

    ui.openContainer(
        rootLayout, {
                        .backgroundColor = SURFACE,
                        .borderColor = OUTLINE,
                        .borderWidth = 2.0f,
                        .borderRadius = 18.0f,
                        .shadowColor = Color(0.0f, 0.0f, 0.0f, 0.6f),
                        .shadowOffset = Vec2(0.0f, 12.0f),
                        .shadowBlurRadius = 28.0f,
                    }
    );
    {
        label(font, "Container styles", TITLE_COLOR, 26.0f);

        UILayoutConfig bodyLayout = box(UISizeSpec::grow(), UISizeSpec::grow());
        bodyLayout.gap = 16.0f;

        ui.openContainer(bodyLayout);
        {
            beginPanel();
            buildBackgroundSection(font, image);
            buildRadiusSection(font);
            buildBorderSection(font);
            ui.closeContainer();

            beginPanel();
            buildBorderStyleSection(font);
            buildShadowSection(font);
            buildLiveSection(font);
            ui.closeContainer();
        }
        ui.closeContainer();

        // The rest of UIContainerStyle is not the quad's business: overflow needs a
        // clip rect from the solver, zIndex reorders the walk, and cursor and
        // transition are input and animation concerns.
        label(
            font, "painted per quad: background, border, radius, dash, shadow", CAPTION_COLOR, 13.0f
        );
    }
    ui.closeContainer();
}

}   // namespace

// Every paintable field of UIContainerStyle, drawn by UIRenderer as one quad each --
// background, border ring, corner radius, dash pattern and shadow all come out of a
// single rounded-box distance field. WASD pans, QE zooms; the edges stay a pixel wide
// at any zoom because the antialiasing is driven by the distance field's gradient.
int ui_test()
{
    IdType windowId = WindowManager::get().createWindow({1600, 800, "UI Test"});

    Registry registry;
    EntityHandle camera = registry.create();
    camera.emplace<TransformComponent>().position = Vec3(ROOT_SIZE.x * 0.5f, ROOT_SIZE.y * 0.5f, 0);
    camera.emplace<CameraComponent>().windowId = windowId;
    camera.get<CameraComponent>().orthoSize = ROOT_SIZE.y * 1.15f;

    Font font(findFontFile(), {.atlasType = FontAtlasType::Mtsdf, .emPixelSize = 48});
    GlTexture image(IMAGE_PATH, 0, GlTexture::FilterMode::Linear, GlTexture::WrapMode::ClampToEdge);

    GlShader quadShader("game/assets/shaders/QuadShader.glsl");
    GlShader textShader("game/assets/shaders/TextShader.glsl");
    GlShader uiShader("game/assets/shaders/UIBoxShader.glsl");
    Renderer::get().init(2000, &quadShader);
    TextRenderer::get().init(&textShader);
    UIRenderer::get().init(&uiShader);

    Input::get().addAxis("Horizontal", {KeyCode::D, KeyCode::A});
    Input::get().addAxis("Vertical", {KeyCode::W, KeyCode::S});
    Input::get().addAxis("Zoom", {KeyCode::E, KeyCode::Q});

    auto onWindowUpdate = [&](IdType id, float dt) {
        TransformComponent& transform = camera.get<TransformComponent>();
        CameraComponent& cam = camera.get<CameraComponent>();

        transform.position.x += Input::get().getAxis("Horizontal") * dt * cam.orthoSize;
        transform.position.y += Input::get().getAxis("Vertical") * dt * cam.orthoSize;
        cam.orthoSize -= Input::get().getAxis("Zoom") * dt * cam.orthoSize;

        buildUi(font, image);
    };

    auto onWindowRender = [&](IdType id, float dt) {
        Renderer::get().beginPass();
        Renderer::get().clearColor(BACKDROP);

        UIManager::get().draw();

        // Boxes and glyphs are separate batches, so the boxes have to resolve before
        // any glyph does or the two draw in whichever order they happen to flush.
        UIRenderer::get().flush();
        TextRenderer::get().flush();

        Renderer::get().beginPass();
        Renderer::get().setShader(&quadShader);
        Renderer::get().drawToWindow();
    };

    Application app(registry);
    app.onWindowUpdate().bind(&onWindowUpdate);
    app.onWindowRender().bind(&onWindowRender);
    app.run();

    return 0;
}
