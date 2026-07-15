#pragma once

#include "components/TilemapComponent.hpp"
#include "graphics/BatchRenderer.hpp"
#include "graphics/TextureAtlas.hpp"
#include "utils/Singleton.hpp"
#include "utils/TypeAliases.hpp"
#include "utils/math/Mat4.hpp"
#include <array>

namespace Engine {

class Tileset;

class TilemapRenderer : public Singleton<TilemapRenderer>
{
    friend class Singleton<TilemapRenderer>;

  private:
    BatchRenderer<TileVertex> m_batch;
    bool m_initialized = false;

  public:
    void init(GlShader* shader, size_t maxQuadCount = 20000);

    // Rebuilds any dirty chunks and draws the whole tilemap into the currently
    // bound render target. windowId names the render context whose per-context
    // VAO is used (VAOs are not shared across contexts). Buffer/window/pass
    // setup is the caller's job.
    void render(TilemapComponent& tilemap, IdType windowId, const Mat4& viewProj);

    size_t totalIndexCount(const TilemapComponent& tilemap) const;

  private:
    TilemapRenderer() = default;
    ~TilemapRenderer() = default;

    void buildChunk(TilemapComponent& tilemap, TilemapChunk& chunk, Tileset& tileset);
    static uint8_t computeNeighborMask(
        TilemapComponent& tilemap, Tileset& tileset, uint16_t selfId, int gx, int gy
    );
    static std::array<Vec2, 4> rotatedUVCorners(const TextureAtlas::Region& region, int rotation);
};

}   // namespace Engine
