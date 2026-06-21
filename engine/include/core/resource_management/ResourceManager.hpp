#pragma once

#include "utils/Singleton.hpp"
#include "utils/IdIndexedVector.hpp"
#include "core/resource_management/IResource.hpp"
#include "core/logging/LoggerMacros.hpp"
#include <cassert>

namespace Engine {

class ResourceManager : public Singleton<ResourceManager>
{
    friend class Singleton<ResourceManager>;

  private:
    IdIndexedVector<IResource*> m_resources;

  public:
    IdType addResource(IResource* resource);
    void unloadResource(IdType id);

    template<typename T, typename... Args> IdType addResource(Args&&... args);
    template<typename T, typename... Args> void setResource(IdType id, Args&&... args);
    template<typename T> T* getResource(IdType id);
    template<typename T> const T* getResource(IdType id) const;

  private:
    ResourceManager() = default;
    ~ResourceManager();
};

template<typename T, typename... Args> IdType ResourceManager::addResource(Args&&... args)
{
    static_assert(
        std::is_base_of<IResource, T>::value,
        "Error: You can only add classes that inherit from IResource!"
    );
    T* res = new T(std::forward<Args>(args)...);
    return m_resources.add(static_cast<IResource*>(res));
}
template<typename T, typename... Args> void ResourceManager::setResource(IdType id, Args&&... args)
{
    T* curr = getResource<T>(id);
    if (!curr) return;
    delete curr;

    T* res = new T(std::forward<Args>(args)...);
    m_resources.set(id, static_cast<IResource*>(res));
}

template<typename T> T* ResourceManager::getResource(IdType id)
{
    if (!m_resources.contains(id)) {
        LOG_WARNING("trying to acces resource with id {} but it doesn't exist", id);
        return nullptr;
    }
    IResource* res = m_resources.get(id);
    assert(dynamic_cast<T*>(res) != nullptr && "Resource type mismatch!");
    return static_cast<T*>(res);
}
template<typename T> const T* ResourceManager::getResource(IdType id) const
{
    if (!m_resources.contains(id)) {
        LOG_WARNING("trying to acces resource with id {} but it doesn't exist", id);
        return nullptr;
    }
    const IResource* res = m_resources.get(id);
    assert(dynamic_cast<const T*>(res) != nullptr && "Resource type mismatch!");
    return static_cast<const T*>(res);
}

}   // namespace Engine
