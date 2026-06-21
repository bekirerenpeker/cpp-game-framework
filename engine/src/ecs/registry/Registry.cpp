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
    m_nextFreeWordIdx = 0;
}
void Registry::clear(Entity e)
{
    for (auto& [typeId, pool] : m_pools) {
        if (pool->contains(e)) pool->remove(e);
    }
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
    m_nextFreeWordIdx = std::min(m_nextFreeWordIdx, index >> 5);
}
void Registry::destroyDeferred(Entity e) { m_entitiesToDestroy.push_back(e); }
void Registry::flush(size_t countToRemove)
{
    size_t cappedCount = (countToRemove == 0) ? m_entitiesToDestroy.size() :
                                                std::min(countToRemove, m_entitiesToDestroy.size());
    for (size_t i = 0; i < cappedCount; ++i) {
        destroy(m_entitiesToDestroy.back());
        m_entitiesToDestroy.pop_back();
    }
}

EntityId Registry::findFreeSlot()
{
    for (size_t i = m_nextFreeWordIdx; i < m_freeList.size(); i++) {
        if (m_freeList[i] == 0) continue;
        m_nextFreeWordIdx = i;
        int offset = std::countr_zero(m_freeList[i]);
        m_freeList[i] &= ~(1u << offset);
        return i * 32 + offset;
    }
    m_freeList.push_back(UINT32_MAX);
    m_freeList.back() &= ~1u;
    m_nextFreeWordIdx = m_freeList.size() - 1;
    return (m_freeList.size() - 1) * 32;
}

}   // namespace Engine
