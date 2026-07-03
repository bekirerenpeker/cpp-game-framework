#include "graphics/tilemap/Tileset.hpp"
#include "core/file_management/ImageFile.hpp"
#include <string>

namespace Engine {

Tileset::Tileset(const fs::path& tilesetImage)
    : m_tilsetImgPath(tilesetImage), m_texture(tilesetImage)
{
}

void Tileset::createTile(const std::string& name, int pixelX, int pixelY, int pixelW, int pixelH)
{
    TileDefinition def;
    def.type = TileType::Normal;
    def.defaultFlags = 0;

    def.uvMin.x = pixelX / (float)m_texture.getWidth();
    def.uvMin.y = pixelY / (float)m_texture.getHeight();
    def.uvMax.x = (pixelX + pixelW) / (float)m_texture.getWidth();
    def.uvMax.y = (pixelY + pixelH) / (float)m_texture.getHeight();

    m_tiles[name] = def;
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
