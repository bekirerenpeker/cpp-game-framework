#include "EngineInclude.hpp"
#include "physics/PhysicsManager.hpp"
#include "test_funcs.hpp"

using namespace Engine;

int physics_test()
{
    IdType windowId = WindowManager::get().createWindow(
        {.width = 1000,
         .height = 800,
         .title = "Physics Test",
         .creationHints = WindowFlags::Transparent}
    );

    Registry registry;
    EntityHandle camera = registry.create();
    camera.emplace<TransformComponent>();
    camera.emplace<CameraComponent>().windowId = windowId;
    camera.get<CameraComponent>().orthoSize = 500;

    Renderer::get().init();
    TextRenderer::get().init();
    UIRenderer::get().init();

    Input::get().addAxis("Horizontal", {KeyCode::D, KeyCode::A});
    Input::get().addAxis("Vertical", {KeyCode::W, KeyCode::S});
    Input::get().addAxis("Zoom", {KeyCode::E, KeyCode::Q});

    auto onFixedUpdate = [&](float dt) { PhysicsManager::get().step(registry, dt); };

    auto onWindowUpdate = [&](IdType winId, float dt) {
        TransformComponent& transform = camera.get<TransformComponent>();
        CameraComponent& cam = camera.get<CameraComponent>();
        transform.position.x += Input::get().getAxis("Horizontal") * dt * cam.orthoSize;
        transform.position.y += Input::get().getAxis("Vertical") * dt * cam.orthoSize;
        cam.orthoSize -= Input::get().getAxis("Zoom") * dt * cam.orthoSize;
    };

    auto onWindowRender = [&](IdType winId, float dt) {
        Renderer::get().clearColor(Color(0.4, 0.1, 0.8));

        Renderer::get().beginScene();
        Renderer::get().endScene();

        UIWidgets::clear();
        UIManager::get().draw();
    };

    Application app(registry);
    app.onFixedUpdate().bind(&onFixedUpdate);
    app.onWindowUpdate().bind(&onWindowUpdate);
    app.onWindowRender().bind(&onWindowRender);
    app.run();

    return 0;
}
