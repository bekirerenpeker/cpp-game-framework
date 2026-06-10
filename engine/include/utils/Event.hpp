#pragma once

#include "logging/Debugger.hpp"
#include <functional>
#include <unordered_map>
#include <vector>

template<typename... Args> class Event
{
  private:
    using FuncType = std::function<void(Args...)>;
    std::vector<FuncType> m_listeners;
    std::unordered_map<size_t, size_t> m_indices;
    size_t m_nextID = 0;

  public:
    int subscribe(FuncType func)
    {
        LOG_INFO("subscribed new func " << (size_t)this);
        m_listeners.push_back(func);
        m_indices[++m_nextID] = m_listeners.size() - 1;
        return m_nextID;
    }

    void unsubscribe(int subID)
    {
        if (!m_indices.count(subID)) return;

        LOG_INFO("removed func " << (size_t)this);
        size_t removedIndex = m_indices[subID];
        m_listeners.erase(m_listeners.begin() + removedIndex);
        m_indices.erase(subID);

        for (auto& [id, index] : m_indices) {
            if (index > removedIndex) index--;
        }
    }

    void call(Args... args) const
    {
        LOG_INFO("calling " << m_listeners.size() << ", " << (size_t)this);
        for (int i = 0; i < m_listeners.size(); i++) m_listeners[i](args...);
    }

    int operator+=(FuncType func) { return subscribe(func); }
    void operator-=(int subID) { unsubscribe(subID); }
    void operator()(Args... args) const { call(args...); }
};

#define BIND_METHOD(func, instance) std::bind(&func, &instance, std::placeholders::_1)
