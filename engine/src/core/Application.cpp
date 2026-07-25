#include "core/Application.hpp"
#include "context/GlfwContext.hpp"
#include "core/Time.hpp"
#include "core/input/Input.hpp"
#include "core/input/KeyCodes.hpp"
#include "core/window_management/ViewContext.hpp"
#include "core/window_management/Window.hpp"
#include "core/window_management/WindowManager.hpp"
#include "ecs/registry/Registry.hpp"

namespace Engine {

void Application::run()
{
    while (WindowManager::get().anyWindowOpen()) {
        m_windowsToClose.clear();
        Time::get().update();
        float dt = Time::get().deltaTime();

        if (m_onFrame.isBound()) m_onFrame(dt);

        for (auto& [windowId, window] : WindowManager::get().getAllWindows()) {
            ViewContext::get().setActiveWindow(windowId);
            Input::get().update();

            if (m_onWindowUpdate.isBound()) m_onWindowUpdate(windowId, dt);

            bool escapeClosed = m_closeOnEscape && Input::get().keyPressed(KeyCode::Escape);
            if (!window->isOpen() || escapeClosed) m_windowsToClose.push_back(windowId);

            // Camera transforms are final only after onWindowUpdate, so the
            // view-proj matrix is baked here rather than before it.
            ViewContext::get().updateCamera(m_registry);
            if (m_onWindowRender.isBound()) m_onWindowRender(windowId, dt);

            window->swapBuffers();
        }

        GlfwContext::pollEvents();
        for (IdType id : m_windowsToClose) WindowManager::get().closeWindow(id);
    }
}

}   // namespace Engine
