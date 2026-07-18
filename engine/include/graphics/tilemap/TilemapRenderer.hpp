#pragma once

#include "components/TilemapComponent.hpp"
#include "graphics/BatchRenderer.hpp"
#include "graphics/TextureAtlas.hpp"
#include "utils/Singleton.hpp"
#include "utils/TypeAliases.hpp"
#include <array>

namespace Engine {

class Tileset;
struct WorldBounds;

class TilemapRenderer : public Singleton<TilemapRenderer>
{
    friend class Singleton<TilemapRenderer>;

  private:
    BatchRenderer<TileVertex> m_batch;
    bool m_initialized = false;

  public:
    void init(GlShader* shader, size_t maxQuadCount = 20000);
    void render(TilemapComponent& tilemap, IdType windowId);

  private:
    TilemapRenderer() = default;
    ~TilemapRenderer() = default;

    void buildChunk(TilemapComponent& tilemap, TilemapChunk& chunk, Tileset& tileset);
    static uint8_t computeNeighborMask(
        TilemapComponent& tilemap, Tileset& tileset, uint16_t selfId, int gx, int gy
    );
    static std::array<Vec2, 4> rotatedUVCorners(const TextureAtlas::Region& region, int rotation);
    static bool chunkVisible(const TilemapChunk& chunk, const WorldBounds& bounds);
};

}   // namespace Engine
