#pragma once

#include "graphics/gl_wrappers/GlBuffer.hpp"
#include "graphics/gl_wrappers/GlTexture.hpp"
#include <cstdint>
#include <unordered_map>

namespace Engine {

struct TileData
{
    uint16_t textureId = 0;
    uint16_t flags = 0;
};

struct TilemapChunk
{
    static constexpr int CHUNK_SIZE = 32;

    int chunkX = 0, chunkY = 0;
    TileData tiles[CHUNK_SIZE * CHUNK_SIZE];

    GlBuffer vbo;
    uint32_t indexCount = 0;
    bool isDirty = false;

    TilemapChunk() : vbo(GlBufferType::VertexBuffer) {}
};

class TilemapManager;

struct TilemapComponent
{
    friend class TilemapManager;

  private:
    static uint64_t getChunkKey(int cx, int cy)
    {
        return (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 32) |
               (static_cast<uint64_t>(static_cast<uint32_t>(cy)) & 0xFFFFFFFFFFF);
    }

    std::unordered_map<uint64_t, TilemapChunk> m_chunks;
    GlTexture* m_texture;
};

}   // namespace Engine
