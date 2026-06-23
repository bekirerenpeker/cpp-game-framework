#include "EngineInclude.hpp"
#include "glad/glad.h"

using namespace Engine;

int main()
{
    Logger::get().addSink<FileSink>("game/output/log.txt");

    IdType windowId = WindowManager::get().createWindow({800, 800, "Graphics Test Window"});
    Window* window = WindowManager::get().getWindow(windowId);

    GlTexture tex("game/assets/images/mario.png");
    GlShader shader("game/assets/shaders/QuadShader.glsl");
    Renderer::get().init(10000, &shader);

    while (window->isOpen()) {
        Input::get().update(windowId);
        Time::get().update();

        if (Input::get().keyPressed(KeyCode::Q)) break;

        Renderer::get().clearColor(Color(0.5f, 0.5f, 1.f, 1.f));
        Renderer::get().beginScene();
        Renderer::get().addQuad(VEC2_ZERO, VEC2_ONE * 0.7f, Color(1), &tex);
        Renderer::get().endScene();

        window->swapBuffers();
        GlfwContext::pollEvents();
    }

    return 0;
}
