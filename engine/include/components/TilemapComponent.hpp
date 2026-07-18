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
    uint16_t textureId = 0;
};

struct TileVertex
{
    Vec2 pos;
    Vec2 uv;
    Color color;
    int texIndex;
};

struct AnimatedTileInstance
{
    float x, y;
    uint16_t tileId;
};

struct TilemapChunk
{
    static constexpr int CHUNK_SIZE = 32;

    int chunkX = 0, chunkY = 0;
    TileData tiles[CHUNK_SIZE * CHUNK_SIZE];

    std::vector<TileVertex> mesh;
    std::vector<AnimatedTileInstance> animatedTiles;
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
