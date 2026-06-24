#include "core/window_management/WindowManager.hpp"
#include "GLFW/glfw3.h"
#include "context/GladContext.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "core/window_management/Window.hpp"
#include "graphics/Renderer.hpp"

namespace Engine {

WindowManager::WindowManager() : m_contextWindow({1, 1, "Context Window", WindowFlags::NotVisible})
{
    glfwMakeContextCurrent(m_contextWindow.getGlfwHandle());
    GladContext::init();
}
WindowManager::~WindowManager()
{
    for (auto& [id, window] : m_windows.getAll()) delete window;
}

IdType WindowManager::createWindow(WindowCreationOptions opts, bool setAsMain)
{
    if (windowCount() >= m_maxWindowCount) {
        LOG_WARNING(
            "couldn't create window {} because the max window count of {} has been reached",
            opts.title, m_maxWindowCount
        );
        return INVALID_ID;
    }

    IdType id = m_windows.add(new Window(opts, nullptr, m_contextWindow.getGlfwHandle()));
    if (setAsMain || m_mainWindowId == INVALID_ID) {
        m_mainWindowId = id;
        Renderer::get().setRenderWindowId(m_mainWindowId);
    }

    return id;
}

void WindowManager::closeWindow(IdType windowId)
{
    // change the current context to the deleted window so every opengl id
    // that was created in this context points to the right object
    Window* win = m_windows.get(windowId);
    Renderer::get().setRenderWindowId(INVALID_ID);
    glfwMakeContextCurrent(win->getGlfwHandle());

    m_windows.remove(windowId);
    if (windowId == m_mainWindowId) {
        m_mainWindowId = m_windows.isEmpty() ? INVALID_ID : m_windows.getAll()[0].first;
    }

    delete win;

    // set the context to the null window to avoid errors
    glfwMakeContextCurrent(m_contextWindow.getGlfwHandle());
}

std::vector<std::pair<IdType, Window*>>& WindowManager::getAllWindows()
{
    return m_windows.getAll();
}

Window* WindowManager::getWindow(IdType windowId) { return m_windows.get(windowId); }

Window* WindowManager::getMainWindow() { return m_windows.get(m_mainWindowId); }

void WindowManager::setMainWindowId(IdType windowId)
{
    if (m_windows.contains(windowId)) m_mainWindowId = windowId;
}
IdType WindowManager::getMainWindowId() const { return m_mainWindowId; }

bool WindowManager::anyWindowOpen() const { return m_windows.size() != 0; }
size_t WindowManager::windowCount() const { return m_windows.size(); }
size_t WindowManager::getMaxWindowCount() const { return m_maxWindowCount; }
void WindowManager::setMaxWindowCount(size_t maxWinCount)
{
    m_maxWindowCount = maxWinCount == 0 ? 1 : maxWinCount;
}

}   // namespace Engine
