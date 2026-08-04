#include "EngineInclude.hpp"
#include "test_funcs.hpp"

using namespace Engine;

namespace {

const char* FONT_FOLDER = "game/assets/fonts";
const char* FALLBACK_FONT = "C:/Windows/Fonts/segoeui.ttf";

const Color BACKDROP(0.05f, 0.05f, 0.07f, 1.0f);

// Same face baked twice, differing only in atlas type, so TAB changes exactly one
// variable about how the UI's text is rasterised.
const Font* g_mtsdfFont = nullptr;
const Font* g_bitmapFont = nullptr;

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

}   // namespace

// A bare harness: everything on screen comes out of UIWidgets::demoWindow(), so the
// scene itself declares no UI of its own. SPACE toggles screen/world UI space, TAB
// swaps the UI font between the mtsdf and bitmap atlases of one face, and WASD/QE pan
// and zoom the camera, which only moves the UI in world space.
int ui_test()
{
    IdType windowId = WindowManager::get().createWindow({1600, 800, "UI Test"});

    Registry registry;
    EntityHandle camera = registry.create();
    // A world-space root hangs down and right of the world origin, so the camera looks
    // at the middle of that box rather than the first quadrant.
    camera.emplace<TransformComponent>().position = Vec3(490.0f, -323.0f, 0);
    camera.emplace<CameraComponent>().windowId = windowId;
    camera.get<CameraComponent>().orthoSize = 743.0f;

    fs::path fontFile = findFontFile();
    Font mtsdfFont(fontFile, {.atlasType = FontAtlasType::Mtsdf, .emPixelSize = 48});
    Font bitmapFont(fontFile, {.atlasType = FontAtlasType::Bitmap, .emPixelSize = 48});
    g_mtsdfFont = &mtsdfFont;
    g_bitmapFont = &bitmapFont;
    UIManager::get().setFont(g_bitmapFont);

    Renderer::get().init();
    TextRenderer::get().init();
    UIRenderer::get().init();

    Input::get().addAxis("Horizontal", {KeyCode::D, KeyCode::A});
    Input::get().addAxis("Vertical", {KeyCode::W, KeyCode::S});
    Input::get().addAxis("Zoom", {KeyCode::E, KeyCode::Q});

    auto onWindowUpdate = [&](IdType id, float dt) {
        TransformComponent& transform = camera.get<TransformComponent>();
        CameraComponent& cam = camera.get<CameraComponent>();

        transform.position.x += Input::get().getAxis("Horizontal") * dt * cam.orthoSize;
        transform.position.y += Input::get().getAxis("Vertical") * dt * cam.orthoSize;
        cam.orthoSize -= Input::get().getAxis("Zoom") * dt * cam.orthoSize;

        if (Input::get().keyPressed(KeyCode::Space)) {
            UIRenderer::get().setSpace(
                UIRenderer::get().getSpace() == UISpace::Screen ? UISpace::World : UISpace::Screen
            );
        }

        if (Input::get().keyPressed(KeyCode::Tab)) {
            UIManager::get().setFont(
                UIManager::get().getFont() == g_mtsdfFont ? g_bitmapFont : g_mtsdfFont
            );
        }
    };

    auto onWindowRender = [&](IdType id, float dt) {
        Renderer::get().beginPass();
        Renderer::get().clearColor(BACKDROP);

        Renderer::get().beginPass();
        Renderer::get().setShader(Renderer::get().getDefaultShader());
        Renderer::get().drawToWindow();

        UIWidgets::clear();
        UIWidgets::demoWindow();
        UIManager::get().draw();
    };

    Application app(registry);
    app.onWindowUpdate().bind(&onWindowUpdate);
    app.onWindowRender().bind(&onWindowRender);
    app.run();

    return 0;
}
