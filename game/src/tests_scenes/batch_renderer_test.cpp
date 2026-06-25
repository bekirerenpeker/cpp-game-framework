#include "EngineInclude.hpp"
#include "test_funcs.hpp"

using namespace Engine;

int batch_renderer_test()
{
    Logger::get().setUseAsync(true);

    IdType windowId1 = WindowManager::get().createWindow(
        {1000, 800, "Graphics Test Window 1", WindowFlags::Transparent}
    );
    IdType windowId2 = WindowManager::get().createWindow(
        {1000, 800, "Graphics Test Window 1", WindowFlags::Transparent}
    );

    Registry registry;
    EntityHandle camera1 = registry.create();
    camera1.emplace<TransformComponent>();
    camera1.emplace<CameraComponent>().windowId = windowId1;
    camera1.get<CameraComponent>().orthoSize = 1000;
    EntityHandle camera2 = registry.create();
    camera2.emplace<TransformComponent>();
    camera2.emplace<CameraComponent>().windowId = windowId2;
    camera2.get<CameraComponent>().orthoSize = 1000;

    GlTexture tex("game/assets/images/mario.png");
    GlShader shader("game/assets/shaders/QuadShader.glsl");
    GlShader ppShader("game/assets/shaders/PostProcessingShader.glsl");
    Renderer::get().init(10000, &shader);

    Input::get().addAxis("Horizontal", {KeyCode::D, KeyCode::A, KeyCode::Right, KeyCode::Left});
    Input::get().addAxis("Vertical", {KeyCode::W, KeyCode::S, KeyCode::Up, KeyCode::Down});
    Input::get().addAxis("Zoom", {KeyCode::E, KeyCode::Q});

    std::vector<IdType> windowsToClose;
    while (WindowManager::get().anyWindowOpen()) {
        windowsToClose.clear();
        Time::get().update();
        float dt = Time::get().deltaTime();
        LOG_INFO("FPS: {}", 1 / dt);

        for (auto& [windowId, window] : WindowManager::get().getAllWindows()) {
            Input::get().update(windowId);

            View<TransformComponent, CameraComponent> camView(registry);
            for (const auto& [ent, trans, cam] : camView) {
                if (cam.windowId != windowId) continue;
                trans.position.x += Input::get().getAxis("Horizontal") * dt * 2 * cam.orthoSize;
                trans.position.y += Input::get().getAxis("Vertical") * dt * 2 * cam.orthoSize;
                cam.orthoSize -= Input::get().getAxis("Zoom") * dt * cam.orthoSize * 2;
            }

            if (Input::get().keyPressed(KeyCode::Escape) || !window->isOpen()) {
                windowsToClose.push_back(windowId);
            }

            Renderer::get().setRenderWindowId(windowId);
            Renderer::get().setViewProjMat(registry);

            Renderer::get().beginPass();
            Renderer::get().setShader(&shader);
            Renderer::get().clearColor(Color(0.5f, 0.5f, 1.0f, 1.0f));
            float size = 5;
            int w = window->getWidth() / size, h = window->getHeight() / size;
            for (int x = 0; x < w; x++) {
                for (int y = 0; y < h; y++) {
                    Renderer::get().addQuad(
                        Vec2(x, y) * size - window->getSize() / 2.f, VEC2_ONE * size,
                        Time::get().currTime() + windowId * PI2,
                        Color(1 - x / (float)w, 1 - y / (float)h, 0, 1), &tex
                    );
                }
            }

            Renderer::get().beginPass();
            Renderer::get().setShader(&ppShader);
            Renderer::get().drawToWindow();

            window->swapBuffers();
        }

        GlfwContext::pollEvents();
        for (const auto& windowId : windowsToClose) WindowManager::get().closeWindow(windowId);
    }

    return 0;
}
