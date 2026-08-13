#pragma once

#include "utils/Singleton.hpp"
#include "utils/TypeAliases.hpp"
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <vector>

namespace Engine {

class LayerManager : public Singleton<LayerManager>
{
    friend class Singleton<LayerManager>;

  private:
    static constexpr uint LAYER_COUNT = sizeof(LayerMask) * 8;

    LayerMask m_rows[LAYER_COUNT];
    std::vector<std::string> m_names;
    std::unordered_map<std::string, LayerId> m_ids;

  public:
    static constexpr uint layerCount() { return LAYER_COUNT; }
    static LayerMask maskOfId(LayerId id);

    LayerId registerLayer(const std::string& name);
    LayerId getLayerId(const std::string& name) const;
    const std::string& getLayerName(LayerId id) const;
    uint getRegisteredCount() const { return static_cast<uint>(m_names.size()); }

    LayerMask maskOf(const std::string& name) const;
    LayerMask maskOf(std::initializer_list<std::string> names) const;

    void setCollision(LayerId a, LayerId b, bool enabled);
    void setCollision(const std::string& a, const std::string& b, bool enabled);
    bool getCollision(LayerId a, LayerId b) const;
    bool getCollision(const std::string& a, const std::string& b) const;

    bool matrixAllows(LayerMask a, LayerMask b) const;

    void reset();

  private:
    LayerManager();
    ~LayerManager() = default;

    bool isValidId(LayerId id, const char* caller) const;
};

}   // namespace Engine
