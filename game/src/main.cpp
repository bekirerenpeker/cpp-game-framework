#include "EngineInclude.hpp"
#include "ecs_test.hpp"
#include "file_management_test.hpp"
#include "piano_demo.hpp"
#include "glad/glad.h"

using namespace Engine;

int main()
{
    Logger::get().addSink<FileSink>("game/output/log.txt");

    IdType windowId = WindowManager::get().createWindow({1000, 800, "Graphics Test Window"});
    Window* window = WindowManager::get().getWindow(windowId);

    while (window->isOpen()) {
        Input::get().update(windowId);
        Time::get().update();

        if (Input::get().keyPressed(KeyCode::Q)) break;

        glBegin(GL_TRIANGLES);
        glColor3f(1, 0, 0);
        glVertex2f(-0.5f, -0.5f);
        glColor3f(0, 1, 0);
        glVertex2f(+0.5f, -0.5f);
        glColor3f(0, 0, 1);
        glVertex2f(+0.0f, +0.5f);
        glEnd();

        window->swapBuffers();
        GlfwContext::pollEvents();
    }

    return 0;
}
