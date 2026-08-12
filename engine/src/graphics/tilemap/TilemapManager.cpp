#include "graphics/tilemap/TilemapManager.hpp"
#include "graphics/tilemap/Tileset.hpp"

namespace Engine {

void TilemapManager::setAt(TilemapComponent& tilemap, int x, int y, const TileData& tile) const
{
    constexpr int S = TilemapChunk::CHUNK_SIZE;

    int chunkX = toChunkCoord(x), chunkY = toChunkCoord(y);
    uint64_t key = chunkKey(chunkX, chunkY);

    auto it = tilemap.chunks.find(key);
    if (it == tilemap.chunks.end()) {
        // A chunk that does not exist already reads back as empty, so materialising one to
        // store the empty tile would cost ~150KB to say nothing.
        if (tile.tileId == 0) return;

        it = tilemap.chunks.try_emplace(key).first;
        it->second.chunkX = chunkX;
        it->second.chunkY = chunkY;
    }

    TilemapChunk& chunk = it->second;
    int index = toLocalIndex(x, y);
    if (chunk.tiles[index].tileId == tile.tileId) return;

    chunk.tiles[index] = tile;
    chunk.isDirty = true;

    // A tile on a chunk edge changes its neighbor chunk's rule-tile baking; a
    // tile on a corner reaches diagonally too, since rule tiles sample all 8
    // neighbors, so the corner-adjacent chunk needs invalidating as well.
    int localX = toLocalCoord(x), localY = toLocalCoord(y);
    if (localX == 0) invalidateChunk(tilemap, chunkX - 1, chunkY);
    if (localX == S - 1) invalidateChunk(tilemap, chunkX + 1, chunkY);
    if (localY == 0) invalidateChunk(tilemap, chunkX, chunkY - 1);
    if (localY == S - 1) invalidateChunk(tilemap, chunkX, chunkY + 1);
    if (localX == 0 && localY == 0) invalidateChunk(tilemap, chunkX - 1, chunkY - 1);
    if (localX == 0 && localY == S - 1) invalidateChunk(tilemap, chunkX - 1, chunkY + 1);
    if (localX == S - 1 && localY == 0) invalidateChunk(tilemap, chunkX + 1, chunkY - 1);
    if (localX == S - 1 && localY == S - 1) invalidateChunk(tilemap, chunkX + 1, chunkY + 1);
}

TileData TilemapManager::getAt(const TilemapComponent& tilemap, int x, int y) const
{
    auto it = tilemap.chunks.find(chunkKey(toChunkCoord(x), toChunkCoord(y)));
    if (it == tilemap.chunks.end()) return TileData {};

    return it->second.tiles[toLocalIndex(x, y)];
}

void TilemapManager::clear(TilemapComponent& tilemap) const { tilemap.chunks.clear(); }

void TilemapManager::invalidateChunk(TilemapComponent& tilemap, int chunkX, int chunkY) const
{
    auto it = tilemap.chunks.find(chunkKey(chunkX, chunkY));
    if (it != tilemap.chunks.end()) it->second.isDirty = true;
}

void TilemapManager::setTileset(TilemapComponent& tilemap, Tileset* tileset) const
{
    tilemap.tileset = tileset;
}

Tileset* TilemapManager::getTileset(const TilemapComponent& tilemap) const
{
    return tilemap.tileset;
}

const Tileset::TileDefinition*
TilemapManager::getDefinitionAt(const TilemapComponent& tilemap, int x, int y) const
{
    if (!tilemap.tileset) return nullptr;
    return tilemap.tileset->getTile(getAt(tilemap, x, y).tileId);
}

const Tileset::TileDefinition*
TilemapManager::getDefinitionAt(const TilemapComponent& tilemap, TileCoord tile) const
{
    return getDefinitionAt(tilemap, tile.x, tile.y);
}

bool TilemapManager::isSolidAt(const TilemapComponent& tilemap, int x, int y) const
{
    const Tileset::TileDefinition* def = getDefinitionAt(tilemap, x, y);
    return def && def->collision != TileCollision::None;
}

bool TilemapManager::isSolidAt(const TilemapComponent& tilemap, TileCoord tile) const
{
    return isSolidAt(tilemap, tile.x, tile.y);
}

}   // namespace Engine
