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

enum class AnimMode : uint8_t
{
    Loop = 0,
    PingPong,
    Once
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
    struct TileAnimation
    {
        std::vector<TextureAtlas::Region> frames;
        float frameDuration = 0.1f;
        AnimMode mode = AnimMode::Loop;
        // When set, each tile starts at a per-position frame offset so identical
        // animated tiles don't play in lockstep.
        bool randomOffset = false;
    };

    TextureAtlas m_atlas;
    std::unordered_map<std::string, TileDefinition> m_tiles;
    // Indexed by tile id; index 0 is the reserved "empty" sentinel, so real
    // tiles start at id 1. Lets the mesh builder resolve a TileData.textureId
    // to its UV rect in O(1).
    std::vector<TileDefinition> m_tilesById {TileDefinition {}};
    // Animation data keyed by tile id, for tiles whose type is Animated.
    std::unordered_map<uint16_t, TileAnimation> m_animations;
    // Region variants keyed by tile id, for tiles whose type is Randomized. One
    // variant is picked per tile position, so it bakes into the static mesh.
    std::unordered_map<uint16_t, std::vector<TextureAtlas::Region>> m_variations;

  public:
    Tileset(const fs::path& tilesetImage);
    ~Tileset() = default;

    // The atlas owns the texture and all partitioning; use it to slice regions
    // (fromCellSize / fromGridSize / fromAutomatic / addRegion), then turn those
    // named regions into tiles with the create* methods below.
    TextureAtlas& getAtlas() { return m_atlas; }
    const TextureAtlas& getAtlas() const { return m_atlas; }

    const GlTexture& getTexture() const { return m_atlas.getTexture(); }

    // Returns nullptr for the empty id (0) or any id that was never created.
    const TileDefinition* getTile(uint16_t id) const;
    // Returns 0 (empty) if no tile with that name exists.
    uint16_t getTileId(const std::string& name) const;
    // Current UV rect for a tile, resolved from its tile position in the tilemap
    // grid. Animated tiles return the frame at `time`; randomized tiles return the
    // variant chosen for tilePos; static tiles ignore both and return their fixed
    // rect. tilePos matters for randomized tiles and for animations created with
    // randomOffset (per-tile phase).
    TextureAtlas::Region getTileUV(uint16_t id, float time, Vec2 tilePos = VEC2_ZERO) const;

    // One tile from an existing atlas region, keyed by that region's name.
    // Returns the new tile's id, or 0 if no such region exists.
    uint16_t createTile(const std::string& regionName);
    // Bulk tiles from regions prefix{minIndex}..prefix{maxIndex-1}.
    void createTiles(const std::string& prefix, int minIndex, int maxIndex);
    // Animated tile cycling through the given frame regions, one frame every
    // frameDuration seconds. Returns the new tile's id. With randomOffset, each
    // placed tile is phase-shifted by its position so a field of the same tile
    // does not animate in lockstep. Frames come from an explicit name list or
    // from framePrefix{minIndex}..{maxIndex-1}.
    uint16_t createAnimatedTile(
        const std::string& name, const std::vector<std::string>& frameNames, float frameDuration,
        AnimMode mode = AnimMode::Loop, bool randomOffset = false
    );
    uint16_t createAnimatedTile(
        const std::string& name, const std::string& framePrefix, int minIndex, int maxIndex,
        float frameDuration, AnimMode mode = AnimMode::Loop, bool randomOffset = false
    );
    // Randomized tile that shows one of the given variant regions, chosen
    // deterministically from each tile's position. Returns the new tile's id.
    // Variants come from an explicit name list or from prefix{minIndex}..{maxIndex-1}.
    uint16_t
    createRandomizedTile(const std::string& name, const std::vector<std::string>& regionNames);
    uint16_t createRandomizedTile(
        const std::string& name, const std::string& prefix, int minIndex, int maxIndex
    );

  private:
    std::vector<TextureAtlas::Region> gatherRegions(const std::vector<std::string>& names) const;
    std::vector<TextureAtlas::Region>
    gatherRegions(const std::string& prefix, int minIndex, int maxIndex) const;
    uint16_t
    registerTile(const std::string& name, const TextureAtlas::Region& region, TileType type);
    uint16_t registerAnimatedTile(const std::string& name, const TileAnimation& anim);
    uint16_t registerRandomizedTile(
        const std::string& name, const std::vector<TextureAtlas::Region>& variants
    );
    static int animFrame(const TileAnimation& anim, float time, Vec2 tilePos);
    static uint32_t hashTilePos(Vec2 tilePos);
};

}   // namespace Engine
