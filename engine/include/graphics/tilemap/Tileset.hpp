#pragma once

#include "core/resource_management/IResource.hpp"
#include "graphics/TextureAtlas.hpp"
#include "utils/math/Vec2.hpp"
#include <string>
#include <unordered_map>
#include <vector>

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
  public:
    struct TileDefinition
    {
        uint16_t id = 0;
        TileType type = TileType::Normal;
        Vec2 uvMin = VEC2_ZERO, uvMax = VEC2_ONE;
        uint16_t defaultFlags = 0;
    };

  private:
    TextureAtlas m_atlas;
    std::unordered_map<std::string, TileDefinition> m_tiles;
    // Indexed by tile id; index 0 is the reserved "empty" sentinel, so real
    // tiles start at id 1. Lets the mesh builder resolve a TileData.textureId
    // to its UV rect in O(1).
    std::vector<TileDefinition> m_tilesById {TileDefinition {}};

  public:
    Tileset(const fs::path& tilesetImage);
    ~Tileset() = default;

    const GlTexture& getTexture() const { return m_atlas.getTexture(); }

    // Returns nullptr for the empty id (0) or any id that was never created.
    const TileDefinition* getTile(uint16_t id) const;
    // Returns 0 (empty) if no tile with that name exists.
    uint16_t getTileId(const std::string& name) const;

    void createTile(const std::string& name, int pixelX, int pixelY, int pixelW, int pixelH);

    void fromGridSize(const std::string& prefix, int gridW, int gridH);
    void fromCellSize(const std::string& prefix, int cellW, int cellH);
    void fromEdges(const std::string& prefix);

  private:
    void registerTile(const std::string& name, const TextureAtlas::Region& region);
};

}   // namespace Engine
