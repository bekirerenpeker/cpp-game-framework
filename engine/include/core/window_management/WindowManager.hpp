#pragma once

#include "utils/Singleton.hpp"
#include "utils/IdIndexedVector.hpp"
#include "core/window_management/Window.hpp"

namespace Engine {

class WindowManager : public Singleton<WindowManager>
{
  private:
    IdIndexedVector<Window> m_windows;
    IdType m_mainWindowId = INVALID_ID;

  public:
    WindowManager() = default;
    ~WindowManager() = default;

    IdType createWindow(WindowCreationOptions opts, bool setAsMain = false);
    void closeWindow(IdType windowId);

    std::vector<std::pair<IdType, Window>>& getAllWindows();
    Window* getWindow(IdType windowId);
    Window* getMainWindow();
    void setMainWindowId(IdType windowId);

    bool hasWindows() const;
};

}   // namespace Engine
