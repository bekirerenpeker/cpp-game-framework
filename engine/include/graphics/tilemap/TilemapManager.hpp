#pragma once

#include "components/TilemapComponent.hpp"
#include "utils/Singleton.hpp"

namespace Engine {

class Tileset;

class TilemapManager : public Singleton<TilemapManager>
{
    friend class Singleton<TilemapManager>;

  public:
    static constexpr int toChunkCoord(int gridValue)
    {
        return gridValue >> TilemapChunk::CHUNK_SHIFT;
    }
    static constexpr int toLocalCoord(int gridValue)
    {
        return gridValue & TilemapChunk::CHUNK_MASK;
    }
    static constexpr int toLocalIndex(int gridX, int gridY)
    {
        return toLocalCoord(gridY) * TilemapChunk::CHUNK_SIZE + toLocalCoord(gridX);
    }
    static constexpr uint64_t chunkKey(int chunkX, int chunkY)
    {
        return (static_cast<uint64_t>(static_cast<uint32_t>(chunkX)) << 32) |
               static_cast<uint64_t>(static_cast<uint32_t>(chunkY));
    }

    void setAt(TilemapComponent& tilemap, int x, int y, const TileData& tile) const;
    TileData getAt(const TilemapComponent& tilemap, int x, int y) const;
    void clear(TilemapComponent& tilemap) const;

    void invalidateChunk(TilemapComponent& tilemap, int chunkX, int chunkY) const;

    void setTileset(TilemapComponent& tilemap, Tileset* tileset) const;
    Tileset* getTileset(const TilemapComponent& tilemap) const;

    bool isSolidAt(const TilemapComponent& tilemap, int x, int y) const;
    bool isSolidAt(const TilemapComponent& tilemap, TileCoord tile) const;

  private:
    TilemapManager() = default;
    ~TilemapManager() = default;
};

}   // namespace Engine
