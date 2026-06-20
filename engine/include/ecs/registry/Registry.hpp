#pragma once

#include "Entity.hpp"
#include "SparseSet.hpp"
#include "utils/TypeAliases.hpp"
#include "utils/TypeRegistery.hpp"
#include <unordered_map>
#include <utility>
#include <vector>

namespace Engine {

class EntityHandle;

class Registry
{
  private:
    std::vector<uint32_t> m_freeList;   // 1 means open 0 means occupied
    std::vector<EntityGen> m_generations;

    std::unordered_map<IdType, ISparseSet*> m_pools;
    std::unordered_map<IdType, void*> m_contexes;

  public:
    Registry() = default;
    ~Registry();

    bool isValid(Entity e);
    void clear(Entity e);
    void clear();

    EntityHandle create();
    void destroy(Entity e);

    template<typename T> SparseSet<T>& getPool()
    {
        IdType typeId = TypeRegistery::get().getTypeId<T>();
        if (!m_pools.count(typeId)) m_pools[typeId] = new SparseSet<T>();
        return *static_cast<SparseSet<T>*>(m_pools[typeId]);
    }
    template<typename T> void clearPool() { return getPool<T>().clear(); }

    template<typename T, typename... Args> void setContext(Args&&... args)
    {
        IdType typeId = TypeRegistery::get().getTypeId<T>();
        if (m_contexes.count(typeId)) delete static_cast<T*>(m_contexes[typeId]);
        m_contexes[typeId] = new T(std::forward<Args>(args)...);
    }
    template<typename T> void removeContext()
    {
        IdType typeId = TypeRegistery::get().getTypeId<T>();
        if (!m_contexes.count(typeId)) return;
        delete static_cast<T*>(m_contexes[typeId]);
        m_contexes.erase(typeId);
    }
    template<typename T> T& getContext()
    {
        IdType typeId = TypeRegistery::get().getTypeId<T>();
        if (!m_contexes.count(typeId)) setContext<T>();
        return *static_cast<T*>(m_contexes[typeId]);
    }
    template<typename T> const T& getContext() const
    {
        IdType typeId = TypeRegistery::get().getTypeId<T>();
        return *static_cast<T*>(m_contexes.at(typeId));
    }

    template<typename T> Sink<void(Entity)> onCreate() { return getPool<T>().onCreate(); }
    template<typename T> Sink<void(Entity)> onDestroy() { return getPool<T>().onDestroy(); }
    template<typename T> Sink<void(Entity)> onSet() { return getPool<T>().onSet(); }

  private:
    EntityId findFreeSlot();
};

}   // namespace Engine
