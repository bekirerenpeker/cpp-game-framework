#include "EngineInclude.hpp"

using namespace Engine;

int main()
{
    WindowManager::get().createWindow({1000, 800, "Test Window 1"});
    WindowManager::get().createWindow({800, 800, "Test Window 2"});
    WindowManager::get().createWindow({800, 1000, "Test Window 3"});

    while (WindowManager::get().hasWindows()) {
        GlfwContext::pollEvents();

        std::vector<IdType> windowsToClose;
        auto& windows = WindowManager::get().getAllWindows();

        for (auto& [id, window] : windows) {
            if (window.isOpen()) {
                Input::get().update(id);

                if (Input::get().keyPressed(KeyCode::Q)) windowsToClose.push_back(id);

                window.swapBuffers();
            } else {
                windowsToClose.push_back(id);
            }
        }

        for (auto& id : windowsToClose) WindowManager::get().closeWindow(id);
    }

    return 0;
}
