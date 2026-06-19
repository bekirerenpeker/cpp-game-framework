#include "EngineInclude.hpp"

using namespace Engine;

int main()
{
    Logger::get().addSink<FileSink>("game/output/log.txt");

    IdType windowId = WindowManager::get().createWindow({1000, 800, "test window"});

    Window* mainWindow = WindowManager::get().getMainWindow();
    while (mainWindow->isOpen()) {
        Time::get().update();
        Input::get().update(windowId);

        if (Input::get().keyPressed(KeyCode::Q)) break;

        mainWindow->swapBuffers();
        GlfwContext::pollEvents();
    }

    return 0;
}
