#include "core/window_management/WindowManager.hpp"
#include "core/logging/LoggerMacros.hpp"

namespace Engine {

IdType WindowManager::createWindow(WindowCreationOptions opts, bool setAsMain)
{
    if (windowCount() >= m_maxWindowCount) {
        LOG_WARNING(
            "couldn't create window {} because the max window count of {} has been reached",
            opts.title, m_maxWindowCount
        );
        return INVALID_ID;
    }
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
IdType WindowManager::getMainWindowId() const { return m_mainWindowId; }

bool WindowManager::anyWindowOpen() const { return m_windows.size() != 0; }
size_t WindowManager::windowCount() const { return m_windows.size(); }
size_t WindowManager::getMaxWindowCount() const { return m_maxWindowCount; }
void WindowManager::setMaxWindowCount(size_t maxWinCount)
{
    m_maxWindowCount = maxWinCount == 0 ? 1 : maxWinCount;
}

}   // namespace Engine
