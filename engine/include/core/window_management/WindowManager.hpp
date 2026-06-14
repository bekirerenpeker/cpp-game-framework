#pragma once

#include "utils/Singleton.hpp"
#include "utils/IdIndexedVector.hpp"
#include "core/window_management/Window.hpp"

namespace Engine {

class WindowManager : public Singleton<WindowManager>
{
    friend class Singleton<WindowManager>;

  private:
    IdIndexedVector<Window> m_windows;
    IdType m_mainWindowId = INVALID_ID;
    size_t m_maxWindowCount = 100;

  public:
    IdType createWindow(WindowCreationOptions opts, bool setAsMain = false);
    void closeWindow(IdType windowId);

    std::vector<std::pair<IdType, Window>>& getAllWindows();
    Window* getWindow(IdType windowId);

    void setMainWindowId(IdType windowId);
    IdType getMainWindowId() const;
    Window* getMainWindow();

    bool anyWindowOpen() const;
    size_t windowCount() const;
    size_t getMaxWindowCount() const;
    void setMaxWindowCount(size_t maxWinCount);

  private:
    WindowManager() = default;
    ~WindowManager() = default;
};

}   // namespace Engine
