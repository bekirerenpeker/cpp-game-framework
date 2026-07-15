#pragma once

#include "core/resource_management/IResource.hpp"
#include "graphics/gl_wrappers/GlTexture.hpp"
#include "utils/math/Vec2.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace Engine {

struct ImageData;

// Slices a single texture into named sub-rectangles (regions) addressable by a
// string key. Holds the source image path so partitioning can read pixels: cell/
// grid slicing skips fully-transparent cells, and automatic slicing flood-fills
// opaque pixels into one region per blob. IResource, like Tileset.
class TextureAtlas : public IResource
{
  public:
    struct Region
    {
        Vec2 uvMin = VEC2_ZERO;
        Vec2 uvMax = VEC2_ONE;
    };

    // Pixel sub-rectangle a partition is restricted to. Default {} is the whole
    // texture; w <= 0 / h <= 0 extend to the texture's right/bottom edge.
    struct Bounds
    {
        int x = 0, y = 0, w = 0, h = 0;
    };

  private:
    fs::path m_imagePath;
    GlTexture m_texture;
    std::unordered_map<std::string, Region> m_regions;

  public:
    TextureAtlas(const fs::path& imagePath);
    ~TextureAtlas() = default;

    const GlTexture& getTexture() const { return m_texture; }

    const Region* getRegion(const std::string& key) const;
    bool hasRegion(const std::string& key) const;

    // Registers a region from pixel coordinates and returns its stored UVs.
    const Region& addRegion(const std::string& key, int pixelX, int pixelY, int pixelW, int pixelH);

    // Slices the (bounds region of the) texture into fixed cellW x cellH cells,
    // skipping cells that are fully transparent. Keys are prefix + a sequential
    // index over the kept cells; returns those keys in order. Without bounds the
    // whole texture is used.
    std::vector<std::string> fromCellSize(const std::string& prefix, int cellW, int cellH);
    std::vector<std::string>
    fromCellSize(const std::string& prefix, int cellW, int cellH, const Bounds& bounds);
    // Derives a cell size from a column/row count and forwards to fromCellSize.
    std::vector<std::string> fromGridSize(const std::string& prefix, int gridCols, int gridRows);
    std::vector<std::string>
    fromGridSize(const std::string& prefix, int gridCols, int gridRows, const Bounds& bounds);
    // Flood-fills connected opaque pixels within bounds, emitting one region (its
    // bounding box) per blob. Keys are prefix + a sequential index; returns them.
    std::vector<std::string> fromAutomatic(const std::string& prefix);
    std::vector<std::string> fromAutomatic(const std::string& prefix, const Bounds& bounds);

  private:
    Bounds resolveBounds(const Bounds& bounds) const;
    bool cellHasOpaquePixels(const ImageData& data, int px, int py, int pw, int ph) const;
};

}   // namespace Engine
