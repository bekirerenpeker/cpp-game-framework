#include "EngineInclude.hpp"

using namespace Engine;

int main()
{
    Logger::get().addSink<FileSink>("game/output/log.txt");

    IdType windowId1 = WindowManager::get().createWindow(
        {600, 600, "Graphics Test Window 1", WindowFlags::Transparent}
    );

    Registry registry;
    EntityHandle camera1 = registry.create();
    EntityHandle camera2 = registry.create();
    camera1.emplace<TransformComponent>();
    camera2.emplace<TransformComponent>();
    camera1.emplace<CameraComponent>().windowId = windowId1;
    //camera2.emplace<CameraComponent>().windowId = windowId2;

    GlTexture tex("game/assets/images/mario.png");
    GlShader shader("game/assets/shaders/QuadShader.glsl");
    GlShader ppShader("game/assets/shaders/PostProcessingShader.glsl");
    GlShader ppShader2("game/assets/shaders/PostProcessingShader_copy.glsl");
    Renderer::get().init(10000, &shader);

    Input::get().addAxis("Horizontal", {KeyCode::D, KeyCode::A, KeyCode::Right, KeyCode::Left});
    Input::get().addAxis("Vertical", {KeyCode::W, KeyCode::S, KeyCode::Up, KeyCode::Down});

    std::vector<IdType> windowsToClose;
    while (WindowManager::get().anyWindowOpen()) {
        windowsToClose.clear();
        Time::get().update();
        float dt = Time::get().deltaTime();

        for (auto& [windowId, window] : WindowManager::get().getAllWindows()) {
            Input::get().update(windowId);

            View<TransformComponent, CameraComponent> camView(registry);
            for (const auto& [ent, trans, cam] : camView) {
                if (cam.windowId != windowId) continue;
                trans.position.x += Input::get().getAxis("Horizontal") * dt * 10;
                trans.position.y += Input::get().getAxis("Vertical") * dt * 10;
            }

            if (Input::get().keyPressed(KeyCode::Q) || !window->isOpen()) {
                windowsToClose.push_back(windowId);
            }

            Renderer::get().setRenderWindowId(windowId);
            Renderer::get().setViewProjMat(registry);

            Renderer::get().beginPass();
            Renderer::get().setShader(&shader);
            Renderer::get().clearColor(Color(0.5f, 0.5f, 1.0f, 1.0f));
            Renderer::get().addQuad(
                VEC2_ONE * -3, VEC2_ONE * 3.7f, Time::get().currTime() + windowId * PI2, Color(1),
                &tex
            );

            Renderer::get().beginPass();
            Renderer::get().setShader(&ppShader);
            Renderer::get().drawToBuffer();

            Renderer::get().beginPass();
            Renderer::get().setShader(&shader);
            Renderer::get().drawToBuffer();
            Renderer::get().addQuad(
                VEC2_ONE * 3, VEC2_ONE * 3.7f, Time::get().currTime() - windowId * PI4, Color(1),
                &tex
            );

            Renderer::get().beginPass();
            Renderer::get().setShader(&ppShader2);
            Renderer::get().drawToWindow();

            window->swapBuffers();
        }

        GlfwContext::pollEvents();
        for (const auto& windowId : windowsToClose) WindowManager::get().closeWindow(windowId);
    }

    return 0;
}
