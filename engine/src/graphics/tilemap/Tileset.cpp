#include "graphics/tilemap/Tileset.hpp"
#include "core/file_management/ImageFile.hpp"
#include <string>

namespace Engine {

Tileset::Tileset(const fs::path& tilesetImage)
    : m_tilsetImgPath(tilesetImage), m_texture(tilesetImage)
{
}

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
    TileDefinition def;
    def.id = static_cast<uint16_t>(m_tilesById.size());
    def.type = TileType::Normal;
    def.defaultFlags = 0;

    def.uvMin.x = pixelX / (float)m_texture.getWidth();
    def.uvMin.y = pixelY / (float)m_texture.getHeight();
    def.uvMax.x = (pixelX + pixelW) / (float)m_texture.getWidth();
    def.uvMax.y = (pixelY + pixelH) / (float)m_texture.getHeight();

    m_tiles[name] = def;
    m_tilesById.push_back(def);
}

void Tileset::fromGridSize(const std::string& prefix, int gridW, int gridH)
{
    int cellW = m_texture.getWidth() / gridW;
    int cellH = m_texture.getHeight() / gridH;
    fromCellSize(prefix, cellW, cellH);
}

void Tileset::fromCellSize(const std::string& prefix, int cellW, int cellH)
{
    int gridW = m_texture.getWidth() / cellW, gridH = m_texture.getHeight() / cellH;
    for (int x = 0; x < gridW; x++) {
        for (int y = 0; y < gridH; y++) {
            createTile(prefix + std::to_string(x + y * gridW), x * cellW, y * cellH, cellW, cellH);
        }
    }
}

void Tileset::fromEdges(const std::string& prefix)
{
    ImageFile img(m_tilsetImgPath);
    ImageData& data = img.getImageData();

    // implement algorithm for automatically selecting tiles using floodfill
}

}   // namespace Engine
