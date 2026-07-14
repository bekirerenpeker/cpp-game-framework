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

void Tileset::createTile(const std::string& name, int pixelX, int pixelY, int pixelW, int pixelH)
{
    registerTile(name, m_atlas.addRegion(name, pixelX, pixelY, pixelW, pixelH));
}

void Tileset::fromGridSize(const std::string& prefix, int gridW, int gridH)
{
    for (const std::string& key : m_atlas.fromGridSize(prefix, gridW, gridH)) {
        registerTile(key, *m_atlas.getRegion(key));
    }
}

void Tileset::fromCellSize(const std::string& prefix, int cellW, int cellH)
{
    for (const std::string& key : m_atlas.fromCellSize(prefix, cellW, cellH)) {
        registerTile(key, *m_atlas.getRegion(key));
    }
}

void Tileset::fromEdges(const std::string& prefix)
{
    for (const std::string& key : m_atlas.fromAutomatic(prefix)) {
        registerTile(key, *m_atlas.getRegion(key));
    }
}

void Tileset::registerTile(const std::string& name, const TextureAtlas::Region& region)
{
    TileDefinition def;
    def.id = static_cast<uint16_t>(m_tilesById.size());
    def.type = TileType::Normal;
    def.defaultFlags = 0;
    def.uvMin = region.uvMin;
    def.uvMax = region.uvMax;

    m_tiles[name] = def;
    m_tilesById.push_back(def);
}

}   // namespace Engine
