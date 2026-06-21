#include "ecs/registry/EntityHandle.hpp"
#include "ecs/registry/Entity.hpp"

namespace Engine {

EntityHandle::EntityHandle(Entity entity, Registry& registry)
    : m_entity(entity), m_registry(registry)
{
}

Entity EntityHandle::getEntity() const { return m_entity; }
EntityId EntityHandle::getId() const { return getEntityId(m_entity); }
EntityGen EntityHandle::getGeneration() const { return getEntityGen(m_entity); }

bool EntityHandle::isValid() { return m_registry.isValid(m_entity); }
void EntityHandle::clear() { return m_registry.clear(m_entity); }
void EntityHandle::destroy()
{
    m_registry.destroy(m_entity);
    m_entity = NULL_ENTITY;
}
EntityHandle EntityHandle::clone() { return m_registry.clone(m_entity); }

}   // namespace Engine
