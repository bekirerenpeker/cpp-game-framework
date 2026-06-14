#include "core/window_management/WindowManager.hpp"

namespace Engine {

IdType WindowManager::createWindow(WindowCreationOptions opts, bool setAsMain)
{
    IdType id = m_windows.add(opts);
    if (setAsMain || m_mainWindowId == INVALID_ID) m_mainWindowId = id;
    return id;
}

void WindowManager::closeWindow(IdType windowId)
{
    m_windows.remove(windowId);
    if (windowId == m_mainWindowId) {
        m_mainWindowId = m_windows.isEmpty() ? INVALID_ID : m_windows.getAll()[0].first;
    }
}

std::vector<std::pair<IdType, Window>>& WindowManager::getAllWindows()
{
    return m_windows.getAll();
}

Window* WindowManager::getWindow(IdType windowId) { return m_windows.get(windowId); }

Window* WindowManager::getMainWindow() { return m_windows.get(m_mainWindowId); }

void WindowManager::setMainWindowId(IdType windowId)
{
    if (m_windows.contains(windowId)) m_mainWindowId = windowId;
}

bool WindowManager::hasWindows() const { return m_windows.size() != 0; }

}   // namespace Engine
