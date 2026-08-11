#include "graphics/tilemap/Tileset.hpp"
#include "core/logging/LoggerMacros.hpp"

namespace Engine {

Tileset::Tileset(const fs::path& tilesetImage) : m_atlas(tilesetImage) {}

const Tileset::TileDefinition* Tileset::getTile(uint16_t id) const
{
    if (id == 0 || id >= m_tiles.size()) return nullptr;
    return &m_tiles[id];
}

uint16_t Tileset::getTileId(const std::string& name) const
{
    auto it = m_tileIds.find(name);
    return it == m_tileIds.end() ? 0 : it->second;
}

void Tileset::setTileSolid(uint16_t id, bool isSolid)
{
    if (id == 0 || id >= m_tiles.size()) {
        LOG_ERROR("setTileSolid({}): no such tile id", id);
        return;
    }
    m_tiles[id].isSolid = isSolid;
}

void Tileset::setTileSolid(const std::string& name, bool isSolid)
{
    uint16_t id = getTileId(name);
    if (id == 0) {
        LOG_ERROR("setTileSolid('{}'): no such tile", name);
        return;
    }
    setTileSolid(id, isSolid);
}

bool Tileset::isTileSolid(uint16_t id) const
{
    const TileDefinition* def = getTile(id);
    return def && def->isSolid;
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
    if (!region) {
        LOG_ERROR("createTile('{}'): no such atlas region; tile not created", regionName);
        return 0;
    }
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
    if (m_tiles.size() > UINT16_MAX) {
        LOG_ERROR("registerTile('{}'): tileset is full at {} tiles", name, m_tiles.size());
        return 0;
    }

    auto [it, inserted] = m_tileIds.try_emplace(name, static_cast<uint16_t>(m_tiles.size()));
    uint16_t id = it->second;

    if (inserted) {
        m_tiles.emplace_back();
    } else {
        // Redefining a name reuses its id, so the old type's side tables have to go with it:
        // getTileUV checks animations and variations before the static rect, and a leftover
        // would keep winning.
        LOG_WARNING("tile '{}' redefined; replacing its definition", name);
        m_animations.erase(id);
        m_variations.erase(id);
        m_rules.erase(id);
    }

    TileDefinition& def = m_tiles[id];
    def = TileDefinition {};
    def.id = id;
    def.type = type;
    def.uvMin = region.uvMin;
    def.uvMax = region.uvMax;
    return id;
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
    int needed = rule.table ? RuleTileTemplateManager::templateRegionCount(*rule.table) : 0;
    if (static_cast<int>(rule.variants.size()) < needed) {
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
