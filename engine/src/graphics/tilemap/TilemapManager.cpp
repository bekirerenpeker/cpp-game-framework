#include "graphics/tilemap/TilemapManager.hpp"
#include "graphics/tilemap/Tileset.hpp"

namespace Engine {

void TilemapManager::setAt(TilemapComponent& tilemap, int x, int y, const TileData& tile)
{
    constexpr int S = TilemapChunk::CHUNK_SIZE;

    // 1. Map global grid coordinates to Chunk space via floor division
    int cx = gridToChunkCoord(x);
    int cy = gridToChunkCoord(y);

    // 2. Compute local coordinates within that specific chunk
    int localX = x - (cx * S);
    int localY = y - (cy * S);

    // 3. Get or lazily insert the chunk
    uint64_t key = TilemapComponent::getChunkKey(cx, cy);
    TilemapChunk& chunk = tilemap.m_chunks[key];

    // If it's a completely new chunk, make sure its positional data is set
    if (chunk.chunkX != cx || chunk.chunkY != cy) {
        chunk.chunkX = cx;
        chunk.chunkY = cy;
    }

    // 4. Update the tile if the data actually differs
    int index = localY * S + localX;
    if (chunk.tiles[index].textureId != tile.textureId || chunk.tiles[index].flags != tile.flags) {
        chunk.tiles[index] = tile;
        chunk.isDirty = true;

        // 5. Cross-Chunk Boundary Invalidation
        // If a tile is modified on the edge of a chunk, its neighbors must rebuild
        // their vertex buffers to adjust for potential rule tile changes. A tile at
        // a chunk corner also reaches diagonally into the corner-adjacent chunk,
        // since rule tiles sample all 8 neighbors, not just the orthogonal 4.
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
    if (it == tilemap.m_chunks.end()) {
        return TileData {};   // Return empty/default tile if chunk hasn't been instantiated
    }

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
