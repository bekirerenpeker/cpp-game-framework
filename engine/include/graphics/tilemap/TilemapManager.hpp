#pragma once

#include "utils/Singleton.hpp"
#include "components/TilemapComponent.hpp"

namespace Engine {

class TilemapManager : public Singleton<TilemapManager>
{
    friend class Singleton<TilemapManager>;

  public:
    void setAt(TilemapComponent& tilemap, int x, int y, const TileData& tile);
    TileData getAt(const TilemapComponent& tilemap, int x, int y);
    void clear(TilemapComponent& tilemap);

    void invalidateChunk(TilemapComponent& tilemap, int cx, int cy);

  private:
    TilemapManager() = default;
    ~TilemapManager() = default;

    static inline int gridToChunkCoord(int value)
    {
        if (value >= 0) return value / TilemapChunk::CHUNK_SIZE;
        else return (value - TilemapChunk::CHUNK_SIZE + 1) / TilemapChunk::CHUNK_SIZE;
    }
};

}   // namespace Engine
