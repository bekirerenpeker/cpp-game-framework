#pragma once

#include "core/resource_management/IResource.hpp"
#include "graphics/TextureAtlas.hpp"
#include "graphics/tilemap/RuleTileTemplates.hpp"
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
    };

  private:
    struct TileAnimation
    {
        std::vector<TextureAtlas::Region> frames;
        float frameDuration = 0.1f;
        AnimMode mode = AnimMode::Loop;
        bool randomOffset = false;
    };

    struct TileRule
    {
        std::vector<TextureAtlas::Region> variants;
        const std::array<RuleTileMapping, 256>* table = nullptr;
    };

    TextureAtlas m_atlas;
    std::unordered_map<std::string, TileDefinition> m_tiles;
    std::vector<TileDefinition> m_tilesById {TileDefinition {}};
    std::unordered_map<uint16_t, TileAnimation> m_animations;
    std::unordered_map<uint16_t, std::vector<TextureAtlas::Region>> m_variations;
    std::unordered_map<uint16_t, TileRule> m_rules;

  public:
    Tileset(const fs::path& tilesetImage);
    ~Tileset() = default;

    TextureAtlas& getAtlas() { return m_atlas; }
    const TextureAtlas& getAtlas() const { return m_atlas; }

    const GlTexture& getTexture() const { return m_atlas.getTexture(); }

    const TileDefinition* getTile(uint16_t id) const;
    uint16_t getTileId(const std::string& name) const;
    TextureAtlas::Region getTileUV(uint16_t id, float time, Vec2 tilePos = VEC2_ZERO) const;

    uint16_t createTile(const std::string& regionName);
    void createTiles(const std::string& prefix, int minIndex, int maxIndex);
    uint16_t createAnimatedTile(
        const std::string& name, const std::vector<std::string>& frameNames, float frameDuration,
        AnimMode mode = AnimMode::Loop, bool randomOffset = false
    );
    uint16_t createAnimatedTile(
        const std::string& name, const std::string& framePrefix, int minIndex, int maxIndex,
        float frameDuration, AnimMode mode = AnimMode::Loop, bool randomOffset = false
    );
    uint16_t
    createRandomizedTile(const std::string& name, const std::vector<std::string>& regionNames);
    uint16_t createRandomizedTile(
        const std::string& name, const std::string& prefix, int minIndex, int maxIndex
    );
    uint16_t createRuleTile(
        const std::string& name, const std::string& prefix, int minIndex, int maxIndex,
        const std::string& templateName
    );

    struct RuleTileUV
    {
        TextureAtlas::Region region;
        int rotation = 0;
    };
    RuleTileUV getRuleTileUV(uint16_t id, uint8_t neighborMask) const;

    bool tilesConnect(uint16_t a, uint16_t b) const;

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
    uint16_t registerRuleTile(const std::string& name, const TileRule& rule);
    static int animFrame(const TileAnimation& anim, float time, Vec2 tilePos);
    static uint32_t hashTilePos(Vec2 tilePos);
};

}   // namespace Engine
