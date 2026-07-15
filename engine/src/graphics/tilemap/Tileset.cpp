#include "graphics/tilemap/Tileset.hpp"

namespace Engine {

Tileset::Tileset(const fs::path& tilesetImage) : m_atlas(tilesetImage) {}

const Tileset::TileDefinition* Tileset::getTile(uint16_t id) const
{
    if (id == 0 || id >= m_tilesById.size()) return nullptr;
    return &m_tilesById[id];
}

uint16_t Tileset::getTileId(const std::string& name) const
{
    auto it = m_tiles.find(name);
    return it == m_tiles.end() ? 0 : it->second.id;
}

TextureAtlas::Region Tileset::getTileUV(uint16_t id, float time, Vec2 worldPos) const
{
    auto anim = m_animations.find(id);
    if (anim != m_animations.end() && !anim->second.frames.empty()) {
        return anim->second.frames[animFrame(anim->second, time, worldPos)];
    }

    const TileDefinition* def = getTile(id);
    if (!def) return {};
    return {def->uvMin, def->uvMax};
}

uint16_t Tileset::createTile(const std::string& regionName)
{
    const TextureAtlas::Region* region = m_atlas.getRegion(regionName);
    if (!region) return 0;
    return registerTile(regionName, *region, TileType::Normal);
}

void Tileset::createTiles(const std::string& prefix, int minIndex, int maxIndex)
{
    for (int i = minIndex; i < maxIndex; i++) createTile(prefix + std::to_string(i));
}

uint16_t Tileset::createAnimatedTile(
    const std::string& name, const std::string& framePrefix, int minIndex, int maxIndex,
    float frameDuration, AnimMode mode, bool randomOffset
)
{
    TileAnimation anim;
    anim.frameDuration = frameDuration;
    anim.mode = mode;
    anim.randomOffset = randomOffset;
    for (int i = minIndex; i < maxIndex; i++) {
        if (const TextureAtlas::Region* region =
                m_atlas.getRegion(framePrefix + std::to_string(i))) {
            anim.frames.push_back(*region);
        }
    }
    return registerAnimatedTile(name, anim);
}

uint16_t
Tileset::registerTile(const std::string& name, const TextureAtlas::Region& region, TileType type)
{
    TileDefinition def;
    def.id = static_cast<uint16_t>(m_tilesById.size());
    def.type = type;
    def.defaultFlags = 0;
    def.uvMin = region.uvMin;
    def.uvMax = region.uvMax;

    m_tiles[name] = def;
    m_tilesById.push_back(def);
    return def.id;
}

uint16_t Tileset::registerAnimatedTile(const std::string& name, const TileAnimation& anim)
{
    // Fall back to the first frame as the static rect so getTile() stays useful.
    TextureAtlas::Region first =
        anim.frames.empty() ? TextureAtlas::Region {} : anim.frames.front();
    uint16_t id = registerTile(name, first, TileType::Animated);
    m_animations[id] = anim;
    return id;
}

int Tileset::animFrame(const TileAnimation& anim, float time, Vec2 worldPos)
{
    int n = static_cast<int>(anim.frames.size());
    if (n <= 1 || anim.frameDuration <= 0.0f) return 0;

    int step = static_cast<int>(time / anim.frameDuration);
    if (step < 0) step = 0;
    if (anim.randomOffset) step += frameOffset(worldPos, n);

    switch (anim.mode) {
    case AnimMode::Once    : return step < n ? step : n - 1;
    case AnimMode::PingPong: {
        int period = 2 * (n - 1);
        int p = step % period;
        return p < n ? p : period - p;
    }
    case AnimMode::Loop:
    default            : return step % n;
    }
}

int Tileset::frameOffset(Vec2 worldPos, int frameCount)
{
    // Deterministic spatial hash so a tile's phase is stable across frames and
    // runs (not Random, which would reshuffle every call).
    uint32_t x = static_cast<uint32_t>(static_cast<int>(worldPos.x));
    uint32_t y = static_cast<uint32_t>(static_cast<int>(worldPos.y));
    uint32_t h = x * 73856093u ^ y * 19349663u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return static_cast<int>(h % static_cast<uint32_t>(frameCount));
}

}   // namespace Engine
