#include "ecs/Registry.hpp"
#include "ecs/Entity.hpp"
#include <bit>

namespace Engine {

Registry::~Registry()
{
    for (auto& [typeId, pool] : m_pools) delete pool;
}

Entity Registry::create()
{
    size_t index = findFreeSlot();
    if (index >= m_generations.size()) m_generations.resize(index + 1, 0);
    return constructEntity(index, m_generations[index]++);
}

void Registry::remove(Entity e)
{
    for (auto& [typeId, pool] : m_pools) pool->remove(e);
    size_t index = getEntityId(e);
    int offset = index & (0b11111);
    m_freeList[index >> 5] |= (1u << offset);
}

size_t Registry::findFreeSlot()
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
