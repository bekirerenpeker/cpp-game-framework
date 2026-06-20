#pragma once

#include "Entity.hpp"
#include "Registry.hpp"
#include <utility>

namespace Engine {

class EntityHandle
{
  private:
    Entity m_entity;
    Registry& m_registry;

  public:
    EntityHandle(Entity entity, Registry& registry);
    ~EntityHandle() = default;

    Entity getEntity() const;
    EntityId getId() const;
    EntityGen getGeneration() const;

    bool isValid();
    void clear();
    void destroy();

    template<typename T, typename... Args> T& emplace(Args&&... args)
    {
        m_registry.getPool<T>().insert(m_entity, T {std::forward<Args>(args)...});
    }
    template<typename T> void remove() { return m_registry.getPool<T>().remove(m_entity); }

    template<typename T> bool contains() { return m_registry.getPool<T>().contains(m_entity); }
    template<typename... T> bool containsAll() { return (contains<T>(m_entity) && ...); }
    template<typename... T> bool containsSome() { return (contains<T>(m_entity) || ...); }

    template<typename T> T& get() { return m_registry.getPool<T>().get(m_entity); }
    template<typename T> const T& get() const { return m_registry.getPool<T>().get(m_entity); }
    template<typename T1, typename T2, typename... Ts> auto get()
    {
        return std::tie(get<T1>(m_entity), get<T2>(m_entity), get<Ts>(m_entity)...);
    }
    template<typename T1, typename T2, typename... Ts> auto get() const
    {
        return std::tie(get<T1>(m_entity), get<T2>(m_entity), get<Ts>(m_entity)...);
    }
};

}   // namespace Engine
