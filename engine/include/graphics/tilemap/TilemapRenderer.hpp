#pragma once

#include "components/TilemapComponent.hpp"
#include "graphics/BatchRenderer.hpp"
#include "graphics/TextureAtlas.hpp"
#include "utils/Singleton.hpp"
#include "utils/TypeAliases.hpp"
#include <array>

namespace Engine {

class Tileset;
class Registry;

class TilemapRenderer : public Singleton<TilemapRenderer>
{
    friend class Singleton<TilemapRenderer>;

  private:
    BatchRenderer<TileVertex> m_batch;
    bool m_initialized = false;
    IdType m_defaultShaderId = INVALID_ID;

  public:
    // A null shader takes the engine's own, which is what almost every caller wants;
    // pass one only to render the tile batch through something else.
    void init(GlShader* shader = nullptr, size_t maxQuadCount = 20000);
    void setShader(GlShader* shader);
    void render(TilemapComponent& tilemap);
    void render(TilemapComponent& tilemap, const TileGrid& grid);
    void render(Registry& registry);

  private:
    TilemapRenderer() = default;
    ~TilemapRenderer() = default;

    void buildChunk(TilemapComponent& tilemap, TilemapChunk& chunk, Tileset& tileset);
    void emitQuad(TilemapChunk& chunk, Vec2 min, Vec2 max, const std::array<Vec2, 4>& uv);
    static uint8_t computeNeighborMask(
        TilemapComponent& tilemap, Tileset& tileset, uint16_t selfId, int gx, int gy
    );
    static std::array<Vec2, 4> rotatedUVCorners(const TextureAtlas::Region& region, int rotation);
    static bool chunkVisible(const TilemapChunk& chunk, const TileGrid& grid);
};

}   // namespace Engine
