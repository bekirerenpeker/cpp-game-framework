#pragma once

#include "Signal.hpp"

namespace Engine {

// Primary template left undefined
template<typename T> class Sink;

template<typename Ret, typename... Args> class Sink<Ret(Args...)>
{
  private:
    Signal<Ret(Args...)>& m_signal;

  public:
    // Construct the sink by passing in the signal it will wrap
    Sink(Signal<Ret(Args...)>& signal) : m_signal(signal) {}

    template<auto Function> void connect() { m_signal.template connect<Function>(); }
    template<auto Method, class C> void connect(C* instance)
    {
        m_signal.template connect<Method, C>(instance);
    }

    template<auto Function> void disconnect() { m_signal.template disconnect<Function>(); }
    template<auto Method, class C> void disconnect(C* instance)
    {
        m_signal.template disconnect<Method, C>(instance);
    }

    bool isEmpty() const { return m_signal.isEmpty(); }
};

}   // namespace Engine
