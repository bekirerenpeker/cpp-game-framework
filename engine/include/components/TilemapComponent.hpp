#pragma once

#include "graphics/Color.hpp"
#include "utils/math/Vec2.hpp"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Engine {

class Tileset;

struct TileData
{
    uint16_t textureId = 0;   // 0 == empty tile (skipped when meshing)
    uint16_t flags = 0;
};

// Per-vertex data for the tilemap mesh. A single tileset texture is bound per
// tilemap, so no per-vertex texture index is needed. Kept intentionally simple
// and expandable (the color channel leaves room for tint/lighting later).
struct TileVertex
{
    Vec2 pos;
    Vec2 uv;
    Color color;
    int texIndex;
};

struct TilemapChunk
{
    static constexpr int CHUNK_SIZE = 32;

    int chunkX = 0, chunkY = 0;
    TileData tiles[CHUNK_SIZE * CHUNK_SIZE];

    // CPU mesh cache, rebuilt by TilemapRenderer only when the chunk is dirty.
    // The GPU upload lives in the batch renderer, so the component is copyable
    // and tile mutation (setAt) needs no GL context.
    std::vector<TileVertex> mesh;
    uint32_t indexCount = 0;
    bool isDirty = false;
};

class TilemapManager;
class TilemapRenderer;

struct TilemapComponent
{
    friend class TilemapManager;
    friend class TilemapRenderer;

  private:
    static uint64_t getChunkKey(int cx, int cy)
    {
        return (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 32) |
               (static_cast<uint64_t>(static_cast<uint32_t>(cy)) & 0xFFFFFFFF);
    }

    std::unordered_map<uint64_t, TilemapChunk> m_chunks;
    Tileset* m_tileset = nullptr;
};

}   // namespace Engine
