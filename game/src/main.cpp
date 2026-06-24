#include "EngineInclude.hpp"
#include "core/window_management/Window.hpp"

using namespace Engine;

int main()
{
    Logger::get().addSink<FileSink>("game/output/log.txt");

    WindowManager::get().createWindow(
        {600, 600, "Graphics Test Window 1", WindowFlags::Transparent}
    );
    WindowManager::get().createWindow(
        {600, 600, "Graphics Test Window 2", WindowFlags::Transparent}
    );

    GlTexture tex("game/assets/images/mario.png");
    GlShader shader("game/assets/shaders/QuadShader.glsl");
    GlShader ppShader("game/assets/shaders/PostProcessingShader.glsl");
    GlShader ppShader2("game/assets/shaders/PostProcessingShader_copy.glsl");
    Renderer::get().init(10000, &shader);

    std::vector<IdType> windowsToClose;
    while (WindowManager::get().anyWindowOpen()) {
        windowsToClose.clear();
        Time::get().update();

        for (auto& [windowId, window] : WindowManager::get().getAllWindows()) {
            Input::get().update(windowId);

            if (Input::get().keyPressed(KeyCode::Q) || !window->isOpen()) {
                windowsToClose.push_back(windowId);
            }

            Renderer::get().setRenderWindowId(windowId);

            Renderer::get().beginPass();
            Renderer::get().setShader(&shader);
            Renderer::get().clearColor(Color(0.5f, 0.5f, 1.0f, 0.7f));
            Renderer::get().addQuad(
                VEC2_ZERO, VEC2_ONE * 0.7f, Time::get().currTime() + windowId * PI2, Color(1), &tex
            );

            Renderer::get().beginPass();
            Renderer::get().setShader(&ppShader);
            Renderer::get().drawToBuffer();

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
