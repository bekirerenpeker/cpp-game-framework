#pragma once

#include "ecs/signals/Delegate.hpp"
#include "utils/TypeAliases.hpp"
#include <vector>

namespace Engine {

class Registry;

// Owns the per-frame/per-window bookkeeping that every windowed scene repeats
// verbatim: the anyWindowOpen loop, Time::update, per-window
// ViewContext::setActiveWindow + Input::update, close-window collection,
// ViewContext::updateCamera (called once camera transforms for the window are
// final, before rendering), swapBuffers, and GlfwContext::pollEvents. The
// render body itself -- shaders, passes, draw calls -- stays scene-owned in
// the bound callbacks. Each phase is optional; an unbound one is skipped.
class Application
{
  private:
    Registry& m_registry;
    bool m_closeOnEscape = true;
    std::vector<IdType> m_windowsToClose;

    Delegate<void(float)> m_onFrame;
    Delegate<void(IdType, float)> m_onWindowUpdate;
    Delegate<void(IdType, float)> m_onWindowRender;

  public:
    explicit Application(Registry& registry) : m_registry(registry) {}

    Delegate<void(float)>& onFrame() { return m_onFrame; }
    Delegate<void(IdType, float)>& onWindowUpdate() { return m_onWindowUpdate; }
    Delegate<void(IdType, float)>& onWindowRender() { return m_onWindowRender; }

    void setCloseOnEscape(bool value) { m_closeOnEscape = value; }

    void run();
};

}   // namespace Engine
