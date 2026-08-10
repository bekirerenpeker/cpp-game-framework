#pragma once

#include "ecs/signals/Delegate.hpp"
#include "utils/TypeAliases.hpp"
#include <vector>

namespace Engine {

class Registry;

class Application
{
  private:
    Registry& m_registry;
    bool m_closeOnEscape = true;
    std::vector<IdType> m_windowsToClose;

    Delegate<void(float)> m_onFrame;
    Delegate<void(float)> m_onFixedUpdate;
    Delegate<void(IdType, float)> m_onWindowUpdate;
    Delegate<void(IdType, float)> m_onWindowRender;

  public:
    explicit Application(Registry& registry) : m_registry(registry) {}

    Delegate<void(float)>& onFrame() { return m_onFrame; }
    Delegate<void(float)>& onFixedUpdate() { return m_onFixedUpdate; }
    Delegate<void(IdType, float)>& onWindowUpdate() { return m_onWindowUpdate; }
    Delegate<void(IdType, float)>& onWindowRender() { return m_onWindowRender; }

    void setCloseOnEscape(bool value) { m_closeOnEscape = value; }

    void run();
};

}   // namespace Engine
