#pragma once

#include "Entity.hpp"
#include "SparseSet.hpp"
#include "utils/TypeAliases.hpp"
#include "utils/TypeRegistery.hpp"
#include <unordered_map>
#include <utility>
#include <vector>

namespace Engine {

class Registry
{
  private:
    std::vector<uint32_t> m_freeList;   // 1 means open 0 means occupied
    std::vector<EntityGen> m_generations;

    std::unordered_map<IdType, ISparseSet*> m_pools;

  public:
    Registry() = default;
    ~Registry();

    bool isValid(Entity e);
    EntityGen currGen(EntityId entityId);

    void clear();
    void clear(Entity e);

    Entity create();
    void destroy(Entity e);

    template<typename T> SparseSet<T>& getPool()
    {
        IdType typeId = TypeRegistery::get().getTypeId<T>();
        if (!m_pools.count(typeId)) m_pools[typeId] = new SparseSet<T>();
        return *static_cast<SparseSet<T>*>(m_pools[typeId]);
    }
    template<typename T> void clearPool() { return getPool<T>().clear(); }
    template<typename T, typename... Args> T& emplace(Entity e, Args&&... args)
    {
        getPool<T>().insert(e, T {std::forward<Args>(args)...});
    }
    template<typename T> void remove(Entity e) { return getPool<T>().remove(e); }
    template<typename T> bool contains(Entity e) { return getPool<T>().contains(e); }
    template<typename T> T& get(Entity e) { return getPool<T>().get(e); }
    template<typename T> const T& get(Entity e) const { return getPool<T>().get(e); }

  private:
    EntityId findFreeSlot();
};

}   // namespace Engine
