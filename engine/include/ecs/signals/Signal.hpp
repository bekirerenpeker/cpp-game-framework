#pragma once

#include "Delegate.hpp"
#include <vector>
#include <algorithm>

namespace Engine {

// Primary template left undefined
template<typename T> class Signal;

template<typename Ret, typename... Args> class Signal<Ret(Args...)>
{
  private:
    using DelegateType = Delegate<Ret(Args...)>;
    std::vector<DelegateType> m_listeners;

  public:
    Signal() = default;
    ~Signal() = default;

    // Connect a free function or static method
    template<auto Function> void connect()
    {
        DelegateType delegate;
        delegate.template bind<Function>();

        if (std::find(m_listeners.begin(), m_listeners.end(), delegate) == m_listeners.end()) {
            m_listeners.push_back(delegate);
        }
    }

    // Connect a class member method
    template<auto Method, class C> void connect(C* instance)
    {
        DelegateType delegate;
        delegate.template bind<Method, C>(instance);

        if (std::find(m_listeners.begin(), m_listeners.end(), delegate) == m_listeners.end()) {
            m_listeners.push_back(delegate);
        }
    }

    // Disconnect a free function
    template<auto Function> void disconnect()
    {
        DelegateType delegate;
        delegate.template bind<Function>();

        m_listeners.erase(
            std::remove(m_listeners.begin(), m_listeners.end(), delegate), m_listeners.end()
        );
    }

    // Disconnect a class member method
    template<auto Method, class C> void disconnect(C* instance)
    {
        DelegateType delegate;
        delegate.template bind<Method, C>(instance);

        m_listeners.erase(
            std::remove(m_listeners.begin(), m_listeners.end(), delegate), m_listeners.end()
        );
    }

    // Publish the event to all connected listeners
    void publish(Args... args) const
    {
        for (size_t i = m_listeners.size(); i > 0; --i) m_listeners[i - 1](args...);
    }

    // Remove all listeners
    void clear() { m_listeners.clear(); }

    // Check if there are any listeners
    bool isEmpty() const { return m_listeners.empty(); }
};

}   // namespace Engine
