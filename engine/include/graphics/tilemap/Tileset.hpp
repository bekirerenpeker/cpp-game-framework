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
    };

    TextureAtlas m_atlas;
    std::unordered_map<std::string, TileDefinition> m_tiles;
    // Indexed by tile id; index 0 is the reserved "empty" sentinel, so real
    // tiles start at id 1. Lets the mesh builder resolve a TileData.textureId
    // to its UV rect in O(1).
    std::vector<TileDefinition> m_tilesById {TileDefinition {}};
    // Animation data keyed by tile id, for tiles whose type is Animated.
    std::unordered_map<uint16_t, TileAnimation> m_animations;

  public:
    Tileset(const fs::path& tilesetImage);
    ~Tileset() = default;

    const GlTexture& getTexture() const { return m_atlas.getTexture(); }

    // Returns nullptr for the empty id (0) or any id that was never created.
    const TileDefinition* getTile(uint16_t id) const;
    // Returns 0 (empty) if no tile with that name exists.
    uint16_t getTileId(const std::string& name) const;
    // Current UV rect for a tile. For animated tiles it's the frame at `time`;
    // for static tiles `time` is ignored and the fixed rect is returned.
    TextureAtlas::Region getTileUV(uint16_t id, float time) const;

    void createTile(const std::string& name, int pixelX, int pixelY, int pixelW, int pixelH);

    // Animated tile cycling through existing atlas regions (by key), one frame
    // every frameDuration seconds. Returns the new tile's id.
    uint16_t createAnimatedTile(
        const std::string& name, const std::vector<std::string>& frameKeys, float frameDuration,
        AnimMode mode = AnimMode::Loop
    );
    // Animated tile whose frames are a horizontal strip of frameCount cells of
    // size frameW x frameH starting at (pixelX, pixelY). Returns the new id.
    uint16_t createAnimatedTileFromStrip(
        const std::string& name, int pixelX, int pixelY, int frameW, int frameH, int frameCount,
        float frameDuration, AnimMode mode = AnimMode::Loop
    );

    void fromGridSize(const std::string& prefix, int gridW, int gridH);
    void fromCellSize(const std::string& prefix, int cellW, int cellH);
    void fromEdges(const std::string& prefix);

  private:
    uint16_t addTile(const std::string& name, const TextureAtlas::Region& region, TileType type);
    uint16_t registerAnimatedTile(const std::string& name, const TileAnimation& anim);
    static int animFrame(const TileAnimation& anim, float time);
};

}   // namespace Engine
