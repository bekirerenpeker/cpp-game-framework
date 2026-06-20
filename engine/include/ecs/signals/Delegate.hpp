#pragma once

#include <utility>
#include <cassert>

namespace Engine {

// Primary template left undefined
template<typename T> class Delegate;

// Partial specialization to unpack the return type and arguments
template<typename Ret, typename... Args> class Delegate<Ret(Args...)>
{
  private:
    // A generic function pointer that takes our type-erased instance (void*) and the arguments
    using StubType = Ret (*)(void*, Args...);

    void* m_instance = nullptr;
    StubType m_stub = nullptr;

    // --- COMPILE-TIME STUB GENERATORS ---
    // The compiler generates a unique, highly optimized version of these functions
    // for every method you bind.

    template<auto Function> static Ret freeFunctionStub(void*, Args... args)
    {
        return Function(std::forward<Args>(args)...);
    }

    template<class C, auto Method> static Ret memberFunctionStub(void* instance, Args... args)
    {
        return (static_cast<C*>(instance)->*Method)(std::forward<Args>(args)...);
    }

  public:
    Delegate() = default;

    // Bind a standard free function or static class method
    template<auto Function> void bind()
    {
        m_instance = nullptr;
        m_stub = &freeFunctionStub<Function>;
    }

    // Bind a class member method
    template<auto Method, class C> void bind(C* instance)
    {
        assert(instance != nullptr && "Cannot bind to a null instance!");
        m_instance = instance;
        m_stub = &memberFunctionStub<C, Method>;
    }

    // Check if the delegate actually has a function bound to it
    bool isBound() const { return m_stub != nullptr; }

    // Invoke the bound function
    Ret operator()(Args... args) const
    {
        assert(isBound() && "Invoking an unbound delegate!");
        return m_stub(m_instance, std::forward<Args>(args)...);
    }

    // The most important part for Signals: Allows us to check if two delegates are identical
    // so we can remove listeners from a list!
    bool operator==(const Delegate& other) const
    {
        return m_instance == other.m_instance && m_stub == other.m_stub;
    }
    bool operator!=(const Delegate& other) const { return !(*this == other); }
};

}   // namespace Engine
