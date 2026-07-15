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

    // Inset by half a texel so UVs never land exactly on a cell boundary; sampled
    // there, floating-point error can round into the neighboring cell and bleed
    // its edge pixels into this tile.
    Region region;
    region.uvMin = Vec2((pixelX + 0.5f) / w, (pixelY + 0.5f) / h);
    region.uvMax = Vec2((pixelX + pixelW - 0.5f) / w, (pixelY + pixelH - 0.5f) / h);

    return m_regions[key] = region;
}

TextureAtlas::Bounds TextureAtlas::resolveBounds(const Bounds& bounds) const
{
    const int texW = m_texture.getWidth(), texH = m_texture.getHeight();

    Bounds r;
    r.x = bounds.x < 0 ? 0 : (bounds.x > texW ? texW : bounds.x);
    r.y = bounds.y < 0 ? 0 : (bounds.y > texH ? texH : bounds.y);
    r.w = bounds.w <= 0 ? texW - r.x : bounds.w;
    r.h = bounds.h <= 0 ? texH - r.y : bounds.h;
    if (r.x + r.w > texW) r.w = texW - r.x;
    if (r.y + r.h > texH) r.h = texH - r.y;
    return r;
}

std::vector<std::string> TextureAtlas::fromCellSize(const std::string& prefix, int cellW, int cellH)
{
    return fromCellSize(prefix, cellW, cellH, Bounds {});
}

std::vector<std::string>
TextureAtlas::fromCellSize(const std::string& prefix, int cellW, int cellH, const Bounds& bounds)
{
    std::vector<std::string> keys;
    if (cellW <= 0 || cellH <= 0) return keys;

    const Bounds b = resolveBounds(bounds);
    const int cols = b.w / cellW;
    const int rows = b.h / cellH;

    ImageFile img(m_imagePath);
    img.loadImage(4);
    const ImageData& data = img.getImageData();

    int index = 0;
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            const int px = b.x + col * cellW, py = b.y + row * cellH;
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
    return fromGridSize(prefix, gridCols, gridRows, Bounds {});
}

std::vector<std::string> TextureAtlas::fromGridSize(
    const std::string& prefix, int gridCols, int gridRows, const Bounds& bounds
)
{
    if (gridCols <= 0 || gridRows <= 0) return {};
    const Bounds b = resolveBounds(bounds);
    return fromCellSize(prefix, b.w / gridCols, b.h / gridRows, b);
}

std::vector<std::string> TextureAtlas::fromAutomatic(const std::string& prefix)
{
    return fromAutomatic(prefix, Bounds {});
}

std::vector<std::string>
TextureAtlas::fromAutomatic(const std::string& prefix, const Bounds& bounds)
{
    std::vector<std::string> keys;

    ImageFile img(m_imagePath);
    img.loadImage(4);
    const ImageData& data = img.getImageData();
    if (!data.pixels || data.depth < 4) return keys;

    const int W = static_cast<int>(data.width);
    const Bounds b = resolveBounds(bounds);
    const int x0 = b.x, y0 = b.y, x1 = b.x + b.w, y1 = b.y + b.h;
    auto opaque = [&](int x, int y) {
        return data.pixels[(static_cast<size_t>(y) * W + x) * data.depth + 3] > 0;
    };

    std::vector<bool> visited(static_cast<size_t>(W) * data.height, false);
    std::vector<std::pair<int, int>> stack;

    int index = 0;
    for (int sy = y0; sy < y1; sy++) {
        for (int sx = x0; sx < x1; sx++) {
            if (visited[static_cast<size_t>(sy) * W + sx] || !opaque(sx, sy)) continue;

            int minX = sx, maxX = sx, minY = sy, maxY = sy;
            stack.clear();
            stack.push_back({sx, sy});
            visited[static_cast<size_t>(sy) * W + sx] = true;

            // 4-connected flood fill (clamped to bounds) so tiles separated by a
            // transparent gap stay distinct; grows the bounding box as it goes.
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
                    if (nx < x0 || ny < y0 || nx >= x1 || ny >= y1) continue;
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
