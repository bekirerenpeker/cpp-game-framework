#pragma once

#include "Entity.hpp"
#include "SparseSet.hpp"
#include "utils/TypeAliases.hpp"
#include "utils/TypeRegistery.hpp"
#include <unordered_map>
#include <vector>

namespace Engine {

class EntityHandle;

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
    void clear(Entity e);
    void clear();

    EntityHandle create();
    void destroy(Entity e);
    void destroy(EntityHandle handle);

    template<typename T> SparseSet<T>& getPool()
    {
        IdType typeId = TypeRegistery::get().getTypeId<T>();
        if (!m_pools.count(typeId)) m_pools[typeId] = new SparseSet<T>();
        return *static_cast<SparseSet<T>*>(m_pools[typeId]);
    }
    template<typename T> void clearPool() { return getPool<T>().clear(); }

  private:
    EntityId findFreeSlot();
};

}   // namespace Engine
