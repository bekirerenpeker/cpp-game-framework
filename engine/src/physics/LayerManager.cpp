#include "physics/LayerManager.hpp"
#include "core/logging/LoggerMacros.hpp"
#include <bit>

namespace Engine {

namespace {

const std::string UNKNOWN_LAYER_NAME = "";

}   // namespace

LayerManager::LayerManager() { reset(); }

void LayerManager::reset()
{
    // Every pair collides until something is switched off, so adding the system changes
    // nothing until a rule is authored.
    for (uint i = 0; i < LAYER_COUNT; i++) m_rows[i] = LAYER_ALL;

    m_names.clear();
    m_ids.clear();
    registerLayer("Default");
}

LayerMask LayerManager::maskOfId(LayerId id)
{
    return id < LAYER_COUNT ? (LayerMask)(1) << id : LAYER_NONE;
}

LayerId LayerManager::registerLayer(const std::string& name)
{
    auto it = m_ids.find(name);
    if (it != m_ids.end()) return it->second;

    if (m_names.size() >= LAYER_COUNT) {
        LOG_ERROR("registerLayer('{}'): all {} layers are taken", name, LAYER_COUNT);
        return 0;
    }

    LayerId id = static_cast<LayerId>(m_names.size());
    m_names.push_back(name);
    m_ids[name] = id;
    return id;
}

LayerId LayerManager::getLayerId(const std::string& name) const
{
    auto it = m_ids.find(name);
    if (it == m_ids.end()) {
        LOG_ERROR("getLayerId('{}'): no such layer", name);
        return 0;
    }
    return it->second;
}

const std::string& LayerManager::getLayerName(LayerId id) const
{
    if (!isValidId(id, "getLayerName")) return UNKNOWN_LAYER_NAME;
    return m_names[id];
}

LayerMask LayerManager::maskOf(const std::string& name) const
{
    auto it = m_ids.find(name);
    if (it == m_ids.end()) {
        LOG_ERROR("maskOf('{}'): no such layer", name);
        return LAYER_NONE;
    }
    return maskOfId(it->second);
}

LayerMask LayerManager::maskOf(std::initializer_list<std::string> names) const
{
    LayerMask mask = LAYER_NONE;
    for (const std::string& name : names) mask |= maskOf(name);
    return mask;
}

void LayerManager::setCollision(LayerId a, LayerId b, bool enabled)
{
    if (!isValidId(a, "setCollision") || !isValidId(b, "setCollision")) return;

    // Written both ways: nothing downstream should have to guess which operand comes first.
    if (enabled) {
        m_rows[a] |= maskOfId(b);
        m_rows[b] |= maskOfId(a);
        return;
    }
    m_rows[a] &= ~maskOfId(b);
    m_rows[b] &= ~maskOfId(a);
}

void LayerManager::setCollision(const std::string& a, const std::string& b, bool enabled)
{
    setCollision(getLayerId(a), getLayerId(b), enabled);
}

bool LayerManager::getCollision(LayerId a, LayerId b) const
{
    if (!isValidId(a, "getCollision") || !isValidId(b, "getCollision")) return false;
    return (m_rows[a] & maskOfId(b)) != 0;
}

bool LayerManager::getCollision(const std::string& a, const std::string& b) const
{
    return getCollision(getLayerId(a), getLayerId(b));
}

// An entity may sit on several layers at once, and any one interacting pair is enough. Walks
// only the bits actually set, which for the usual one-layer entity is a single iteration.
bool LayerManager::matrixAllows(LayerMask a, LayerMask b) const
{
    while (a) {
        LayerId id = static_cast<LayerId>(std::countr_zero(a));
        if (id < LAYER_COUNT && (m_rows[id] & b)) return true;
        a &= a - 1;
    }
    return false;
}

bool LayerManager::isValidId(LayerId id, const char* caller) const
{
    if (id < m_names.size()) return true;
    LOG_ERROR("{}({}): no such layer id", caller, id);
    return false;
}

}   // namespace Engine
