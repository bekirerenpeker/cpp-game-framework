#include "ecs/registry/Registry.hpp"
#include "ecs/registry/EntityHandle.hpp"
#include <bit>

namespace Engine {

Registry::~Registry()
{
    for (auto& [typeId, pool] : m_pools) delete pool;
}

void Registry::clear()
{
    for (auto& [typeId, pool] : m_pools) delete pool;
    m_pools.clear();
    m_freeList.clear();
    m_generations.clear();
}
void Registry::clear(Entity e)
{
    for (auto& [typeId, pool] : m_pools) pool->remove(e);
}

bool Registry::isValid(Entity e)
{
    uint32_t id = getEntityId(e);
    if (id >= m_generations.size()) return false;
    return m_generations[id] == getEntityGen(e);
}

EntityHandle Registry::create()
{
    EntityId index = findFreeSlot();
    if (index >= m_generations.size()) m_generations.resize(index + 1, 0);
    return EntityHandle(constructEntity(index, m_generations[index]++), *this);
}
EntityHandle Registry::clone(Entity e)
{
    EntityHandle newEnt = create();
    for (auto& [id, pool] : m_pools) pool->clone(e, newEnt.getEntity());
    return newEnt;
}

void Registry::destroy(Entity e)
{
    clear(e);
    EntityId index = getEntityId(e);
    int offset = index & (0b11111);
    m_freeList[index >> 5] |= (1u << offset);
}
void Registry::destroyDeferred(Entity e) { m_entitiesToDestroy.push_back(e); }
void Registry::flush()
{
    for (Entity e : m_entitiesToDestroy) destroy(e);
    m_entitiesToDestroy.clear();
}

EntityId Registry::findFreeSlot()
{
    for (size_t i = 0; i < m_freeList.size(); i++) {
        if (m_freeList[i] == 0) continue;
        int offset = std::countr_zero(m_freeList[i]);
        m_freeList[i] &= ~(1u << offset);
        return i * 32 + offset;
    }
    m_freeList.push_back(UINT32_MAX);
    return (m_freeList.size() - 1) * 32;
}

}   // namespace Engine
