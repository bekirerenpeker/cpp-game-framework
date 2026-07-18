#include "graphics/tilemap/Tileset.hpp"
#include "core/logging/LoggerMacros.hpp"

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

TextureAtlas::Region Tileset::getTileUV(uint16_t id, float time, Vec2 tilePos) const
{
    auto anim = m_animations.find(id);
    if (anim != m_animations.end() && !anim->second.frames.empty()) {
        return anim->second.frames[animFrame(anim->second, time, tilePos)];
    }

    auto vars = m_variations.find(id);
    if (vars != m_variations.end() && !vars->second.empty()) {
        return vars->second[hashTilePos(tilePos) % vars->second.size()];
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
    const std::string& name, const std::vector<std::string>& frameNames, float frameDuration,
    AnimMode mode, bool randomOffset
)
{
    TileAnimation anim;
    anim.frameDuration = frameDuration;
    anim.mode = mode;
    anim.randomOffset = randomOffset;
    anim.frames = gatherRegions(frameNames);
    return registerAnimatedTile(name, anim);
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
    anim.frames = gatherRegions(framePrefix, minIndex, maxIndex);
    return registerAnimatedTile(name, anim);
}

uint16_t
Tileset::createRandomizedTile(const std::string& name, const std::vector<std::string>& regionNames)
{
    return registerRandomizedTile(name, gatherRegions(regionNames));
}

uint16_t Tileset::createRandomizedTile(
    const std::string& name, const std::string& prefix, int minIndex, int maxIndex
)
{
    return registerRandomizedTile(name, gatherRegions(prefix, minIndex, maxIndex));
}

uint16_t Tileset::createRuleTile(
    const std::string& name, const std::string& prefix, int minIndex, int maxIndex,
    const std::string& templateName
)
{
    TileRule rule;
    rule.table = &RuleTileTemplateManager::get().getTemplate(templateName);
    rule.variants = gatherRegions(prefix, minIndex, maxIndex);
    return registerRuleTile(name, rule);
}

std::vector<TextureAtlas::Region>
Tileset::gatherRegions(const std::vector<std::string>& names) const
{
    std::vector<TextureAtlas::Region> regions;
    for (const std::string& name : names) {
        if (const TextureAtlas::Region* region = m_atlas.getRegion(name))
            regions.push_back(*region);
    }
    return regions;
}

std::vector<TextureAtlas::Region>
Tileset::gatherRegions(const std::string& prefix, int minIndex, int maxIndex) const
{
    std::vector<TextureAtlas::Region> regions;
    for (int i = minIndex; i < maxIndex; i++) {
        if (const TextureAtlas::Region* region = m_atlas.getRegion(prefix + std::to_string(i))) {
            regions.push_back(*region);
        }
    }
    return regions;
}

uint16_t
Tileset::registerTile(const std::string& name, const TextureAtlas::Region& region, TileType type)
{
    TileDefinition def;
    def.id = static_cast<uint16_t>(m_tilesById.size());
    def.type = type;
    def.uvMin = region.uvMin;
    def.uvMax = region.uvMax;

    m_tiles[name] = def;
    m_tilesById.push_back(def);
    return def.id;
}

uint16_t Tileset::registerAnimatedTile(const std::string& name, const TileAnimation& anim)
{
    if (anim.frames.empty()) {
        LOG_ERROR("createAnimatedTile('{}'): no frame regions found; tile not created", name);
        return 0;
    }

    // Fall back to the first frame as the static rect so getTile() stays useful.
    uint16_t id = registerTile(name, anim.frames.front(), TileType::Animated);
    m_animations[id] = anim;
    return id;
}

uint16_t Tileset::registerRandomizedTile(
    const std::string& name, const std::vector<TextureAtlas::Region>& variants
)
{
    if (variants.empty()) {
        LOG_ERROR("createRandomizedTile('{}'): no variant regions found; tile not created", name);
        return 0;
    }

    // The first variant doubles as the static rect so getTile() stays useful.
    uint16_t id = registerTile(name, variants.front(), TileType::Randomized);
    m_variations[id] = variants;
    return id;
}

uint16_t Tileset::registerRuleTile(const std::string& name, const TileRule& rule)
{
    if (rule.variants.empty()) {
        LOG_ERROR("createRuleTile('{}'): no variant regions found; tile not created", name);
        return 0;
    }

    // The template only references region indices up to some maximum; warn if the
    // variant list can't cover them (out-of-range indices fall back to variant 0).
    int needed = 0;
    if (rule.table) {
        for (const RuleTileMapping& m : *rule.table) {
            if (m.regionIndex >= needed) needed = m.regionIndex + 1;
        }
    }
    if (needed > 0 && static_cast<int>(rule.variants.size()) < needed) {
        LOG_WARNING(
            "createRuleTile('{}'): got {} regions, template references up to {}", name,
            rule.variants.size(), needed
        );
    }

    // The first variant doubles as the static rect so getTile() stays useful.
    uint16_t id = registerTile(name, rule.variants.front(), TileType::Rule);
    m_rules[id] = rule;
    return id;
}

int Tileset::animFrame(const TileAnimation& anim, float time, Vec2 tilePos)
{
    int n = static_cast<int>(anim.frames.size());
    if (n <= 1 || anim.frameDuration <= 0.0f) return 0;

    int step = static_cast<int>(time / anim.frameDuration);
    if (step < 0) step = 0;
    if (anim.randomOffset) step += hashTilePos(tilePos) % static_cast<uint32_t>(n);

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

Tileset::RuleTileUV Tileset::getRuleTileUV(uint16_t id, uint8_t neighborMask) const
{
    auto it = m_rules.find(id);
    if (it == m_rules.end() || it->second.variants.empty() || !it->second.table) return {};

    RuleTileMapping mapping = (*it->second.table)[neighborMask];
    uint8_t idx = mapping.regionIndex < it->second.variants.size() ? mapping.regionIndex : 0;
    return {it->second.variants[idx], mapping.rotation};
}

bool Tileset::tilesConnect(uint16_t a, uint16_t b) const { return a != 0 && a == b; }

uint32_t Tileset::hashTilePos(Vec2 tilePos)
{
    // Deterministic spatial hash so a tile's pick/phase is stable across frames
    // and runs (not Random, which would reshuffle every call). Callers take it
    // mod their option count.
    uint32_t x = static_cast<uint32_t>(static_cast<int>(tilePos.x));
    uint32_t y = static_cast<uint32_t>(static_cast<int>(tilePos.y));
    uint32_t h = x * 73856093u ^ y * 19349663u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return h;
}

}   // namespace Engine
