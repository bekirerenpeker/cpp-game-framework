#include "graphics/tilemap/TilemapManager.hpp"
#include "graphics/tilemap/Tileset.hpp"

namespace Engine {

void TilemapManager::setAt(TilemapComponent& tilemap, int x, int y, const TileData& tile)
{
    constexpr int S = TilemapChunk::CHUNK_SIZE;

    int cx = gridToChunkCoord(x);
    int cy = gridToChunkCoord(y);

    int localX = x - (cx * S);
    int localY = y - (cy * S);

    uint64_t key = TilemapComponent::getChunkKey(cx, cy);
    TilemapChunk& chunk = tilemap.m_chunks[key];

    if (chunk.chunkX != cx || chunk.chunkY != cy) {
        chunk.chunkX = cx;
        chunk.chunkY = cy;
    }

    int index = localY * S + localX;
    if (chunk.tiles[index].textureId != tile.textureId) {
        chunk.tiles[index] = tile;
        chunk.isDirty = true;

        // A tile on a chunk edge changes its neighbor chunk's rule-tile baking; a
        // tile on a corner reaches diagonally too, since rule tiles sample all 8
        // neighbors, so the corner-adjacent chunk needs invalidating as well.
        if (localX == 0) invalidateChunk(tilemap, cx - 1, cy);
        if (localX == S - 1) invalidateChunk(tilemap, cx + 1, cy);
        if (localY == 0) invalidateChunk(tilemap, cx, cy - 1);
        if (localY == S - 1) invalidateChunk(tilemap, cx, cy + 1);
        if (localX == 0 && localY == 0) invalidateChunk(tilemap, cx - 1, cy - 1);
        if (localX == 0 && localY == S - 1) invalidateChunk(tilemap, cx - 1, cy + 1);
        if (localX == S - 1 && localY == 0) invalidateChunk(tilemap, cx + 1, cy - 1);
        if (localX == S - 1 && localY == S - 1) invalidateChunk(tilemap, cx + 1, cy + 1);
    }
}

TileData TilemapManager::getAt(const TilemapComponent& tilemap, int x, int y)
{
    constexpr int S = TilemapChunk::CHUNK_SIZE;

    int cx = gridToChunkCoord(x);
    int cy = gridToChunkCoord(y);

    uint64_t key = TilemapComponent::getChunkKey(cx, cy);

    auto it = tilemap.m_chunks.find(key);
    if (it == tilemap.m_chunks.end()) return TileData {};

    int localX = x - (cx * S);
    int localY = y - (cy * S);

    return it->second.tiles[localY * S + localX];
}

void TilemapManager::clear(TilemapComponent& tilemap) { tilemap.m_chunks.clear(); }

void TilemapManager::invalidateChunk(TilemapComponent& tilemap, int cx, int cy)
{
    uint64_t key = TilemapComponent::getChunkKey(cx, cy);
    auto it = tilemap.m_chunks.find(key);
    if (it != tilemap.m_chunks.end()) it->second.isDirty = true;
}

void TilemapManager::setTileset(TilemapComponent& tilemap, Tileset* tileset)
{
    tilemap.m_tileset = tileset;
}

}   // namespace Engine
