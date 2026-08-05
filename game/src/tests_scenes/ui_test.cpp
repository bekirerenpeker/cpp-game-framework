#include "EngineInclude.hpp"
#include "test_funcs.hpp"

using namespace Engine;

namespace {

const Color BACKDROP(0.05f, 0.05f, 0.07f, 1.0f);

}   // namespace

// A bare harness: everything on screen comes out of UIWidgets::demoWindow(), and nothing
// here loads or configures anything -- no font, no theme, no shader. The UI's font is
// left unset, which resolves to FontLoader's default. SPACE toggles screen/world UI
// space, and WASD/QE pan and zoom the camera, which only moves the UI in world space.
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
