#include "core/resource_management/ResourceManager.hpp"

namespace Engine {

ResourceManager::~ResourceManager()
{
    for (auto& [id, resource] : m_resources.getAll()) delete resource;
}

IdType ResourceManager::addResource(IResource* resource) { return m_resources.add(resource); }

void ResourceManager::unloadResource(IdType id)
{
    if (!m_resources.contains(id)) {
        LOG_WARNING("trying to unload resource with id {} but it doesn't exist", id);
        return;
    }
    delete m_resources.get(id);
    m_resources.remove(id);
}

}   // namespace Engine
