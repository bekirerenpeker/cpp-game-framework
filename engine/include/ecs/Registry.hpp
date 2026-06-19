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
    std::vector<uint16_t> m_generations;

    std::unordered_map<IdType, ISparseSet*> m_pools;

  public:
    Registry() = default;
    ~Registry();

    Entity create();
    void remove(Entity e);

    template<typename T> SparseSet<T>& getPool()
    {
        IdType typeId = TypeRegistery::get().getTypeId<T>();
        if (!m_pools.count(typeId)) m_pools[typeId] = new SparseSet<T>();
        return *static_cast<SparseSet<T>*>(m_pools[typeId]);
    }

    template<typename T, typename... Args> T& emplace(Entity e, Args&&... args)
    {
        getPool<T>().insert(e, T {std::forward<Args>(args)...});
    }

  private:
    size_t findFreeSlot();
};

}   // namespace Engine
