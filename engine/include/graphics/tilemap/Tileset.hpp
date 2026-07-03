#pragma once

#include "core/resource_management/IResource.hpp"
#include "graphics/gl_wrappers/GlTexture.hpp"
#include "utils/math/Vec2.hpp"
#include <unordered_map>

namespace Engine {

enum class TileType : uint8_t
{
    Normal = 0,
    Animated,
    Rule,
    Randomized
};

class Tileset : public IResource
{
  private:
    struct TileDefinition
    {
        uint16_t id = 0;
        TileType type = TileType::Normal;
        Vec2 uvMin = VEC2_ZERO, uvMax = VEC2_ONE;
        uint16_t defaultFlags = 0;
    };

    fs::path m_tilsetImgPath;
    GlTexture m_texture;
    std::unordered_map<std::string, TileDefinition> m_tiles;

  public:
    Tileset(const fs::path& tilesetImage);
    ~Tileset() = default;

    const GlTexture& getTexture() const { return m_texture; }

    void createTile(const std::string& name, int pixelX, int pixelY, int pixelW, int pixelH);

    void fromGridSize(const std::string& prefix, int gridW, int gridH);
    void fromCellSize(const std::string& prefix, int cellW, int cellH);
    void fromEdges(const std::string& prefix);
};

}   // namespace Engine
