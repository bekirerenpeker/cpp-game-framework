#include "graphics/TextureAtlas.hpp"
#include "core/file_management/ImageFile.hpp"
#include <utility>

namespace Engine {

TextureAtlas::TextureAtlas(const fs::path& imagePath) : m_imagePath(imagePath), m_texture(imagePath)
{
}

const TextureAtlas::Region* TextureAtlas::getRegion(const std::string& key) const
{
    auto it = m_regions.find(key);
    return it == m_regions.end() ? nullptr : &it->second;
}

bool TextureAtlas::hasRegion(const std::string& key) const { return m_regions.count(key) != 0; }

const TextureAtlas::Region&
TextureAtlas::addRegion(const std::string& key, int pixelX, int pixelY, int pixelW, int pixelH)
{
    const float w = static_cast<float>(m_texture.getWidth());
    const float h = static_cast<float>(m_texture.getHeight());

    Region region;
    region.uvMin = Vec2(pixelX / w, pixelY / h);
    region.uvMax = Vec2((pixelX + pixelW) / w, (pixelY + pixelH) / h);

    return m_regions[key] = region;
}

std::vector<std::string> TextureAtlas::fromCellSize(const std::string& prefix, int cellW, int cellH)
{
    std::vector<std::string> keys;
    if (cellW <= 0 || cellH <= 0) return keys;

    const int cols = m_texture.getWidth() / cellW;
    const int rows = m_texture.getHeight() / cellH;

    ImageFile img(m_imagePath);
    img.loadImage(4);
    const ImageData& data = img.getImageData();

    int index = 0;
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            const int px = col * cellW, py = row * cellH;
            if (!cellHasOpaquePixels(data, px, py, cellW, cellH)) continue;

            std::string key = prefix + std::to_string(index++);
            addRegion(key, px, py, cellW, cellH);
            keys.push_back(std::move(key));
        }
    }
    return keys;
}

std::vector<std::string>
TextureAtlas::fromGridSize(const std::string& prefix, int gridCols, int gridRows)
{
    if (gridCols <= 0 || gridRows <= 0) return {};
    return fromCellSize(prefix, m_texture.getWidth() / gridCols, m_texture.getHeight() / gridRows);
}

std::vector<std::string> TextureAtlas::fromAutomatic(const std::string& prefix)
{
    std::vector<std::string> keys;

    ImageFile img(m_imagePath);
    img.loadImage(4);
    const ImageData& data = img.getImageData();
    if (!data.pixels || data.depth < 4) return keys;

    const int W = static_cast<int>(data.width), H = static_cast<int>(data.height);
    auto opaque = [&](int x, int y) {
        return data.pixels[(static_cast<size_t>(y) * W + x) * data.depth + 3] > 0;
    };

    std::vector<bool> visited(static_cast<size_t>(W) * H, false);
    std::vector<std::pair<int, int>> stack;

    int index = 0;
    for (int sy = 0; sy < H; sy++) {
        for (int sx = 0; sx < W; sx++) {
            if (visited[static_cast<size_t>(sy) * W + sx] || !opaque(sx, sy)) continue;

            int minX = sx, maxX = sx, minY = sy, maxY = sy;
            stack.clear();
            stack.push_back({sx, sy});
            visited[static_cast<size_t>(sy) * W + sx] = true;

            // 4-connected flood fill so tiles separated by a transparent gap stay
            // distinct; grows the bounding box as it goes.
            while (!stack.empty()) {
                auto [cx, cy] = stack.back();
                stack.pop_back();

                if (cx < minX) minX = cx;
                if (cx > maxX) maxX = cx;
                if (cy < minY) minY = cy;
                if (cy > maxY) maxY = cy;

                const int dx[] = {1, -1, 0, 0};
                const int dy[] = {0, 0, 1, -1};
                for (int d = 0; d < 4; d++) {
                    const int nx = cx + dx[d], ny = cy + dy[d];
                    if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                    const size_t ni = static_cast<size_t>(ny) * W + nx;
                    if (visited[ni] || !opaque(nx, ny)) continue;
                    visited[ni] = true;
                    stack.push_back({nx, ny});
                }
            }

            std::string key = prefix + std::to_string(index++);
            addRegion(key, minX, minY, maxX - minX + 1, maxY - minY + 1);
            keys.push_back(std::move(key));
        }
    }
    return keys;
}

bool TextureAtlas::cellHasOpaquePixels(const ImageData& data, int px, int py, int pw, int ph) const
{
    if (!data.pixels || data.depth < 4) return true;   // no alpha channel -> treat as opaque

    const int W = static_cast<int>(data.width), H = static_cast<int>(data.height);
    for (int y = py; y < py + ph && y < H; y++) {
        for (int x = px; x < px + pw && x < W; x++) {
            if (data.pixels[(static_cast<size_t>(y) * W + x) * data.depth + 3] > 0) return true;
        }
    }
    return false;
}

}   // namespace Engine
