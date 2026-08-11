#pragma once

#include "components/TransformComponent.hpp"
#include "graphics/Color.hpp"
#include "utils/math/MathFuncs.hpp"
#include "utils/math/Vec2.hpp"
#include <bit>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Engine {

class Tileset;

struct TileData
{
    uint16_t tileId = 0;
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
    Vec2 pos;
    uint16_t tileId;
};

struct TileCoord
{
    int x = 0, y = 0;
};

struct TileGrid
{
    Vec2 origin = VEC2_ZERO;
    Vec2 tileSize = VEC2_ONE;

    static TileGrid fromTransform(const TransformComponent& transform)
    {
        return {transform.position, transform.scale.abs()};
    }

    bool isValid() const { return tileSize.x > 0.0f && tileSize.y > 0.0f; }

    Vec2 toLocal(Vec2 world) const { return (world - origin) / tileSize; }

    TileCoord toTile(Vec2 world) const
    {
        Vec2 tile = toLocal(world).floor();
        return {
            static_cast<int>(Math::clamp(tile.x, -TILE_COORD_LIMIT, TILE_COORD_LIMIT)),
            static_cast<int>(Math::clamp(tile.y, -TILE_COORD_LIMIT, TILE_COORD_LIMIT))
        };
    }

    Vec2 tileMin(int x, int y) const
    {
        return origin + Vec2(static_cast<float>(x), static_cast<float>(y)) * tileSize;
    }
    Vec2 tileMax(int x, int y) const { return tileMin(x + 1, y + 1); }

  private:
    static constexpr float TILE_COORD_LIMIT = 1e9f;
};

struct TilemapChunk
{
    static constexpr int CHUNK_SIZE = 32;
    static constexpr int CHUNK_MASK = CHUNK_SIZE - 1;
    static constexpr int CHUNK_SHIFT = std::countr_zero(static_cast<unsigned>(CHUNK_SIZE));
    static_assert(std::has_single_bit(static_cast<unsigned>(CHUNK_SIZE)));

    int chunkX = 0, chunkY = 0;
    TileData tiles[CHUNK_SIZE * CHUNK_SIZE];

    std::vector<TileVertex> mesh;
    std::vector<AnimatedTileInstance> animatedTiles;
    bool isDirty = true;
};

struct TilemapComponent
{
    std::unordered_map<uint64_t, TilemapChunk> chunks;
    Tileset* tileset = nullptr;
};

}   // namespace Engine
