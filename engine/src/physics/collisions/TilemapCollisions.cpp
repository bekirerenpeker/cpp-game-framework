#include "graphics/tilemap/TilemapManager.hpp"
#include "physics/Collisions.hpp"
#include "utils/math/MathFuncs.hpp"

namespace Engine {

namespace Collisions {

namespace {

// A query the size of the world would otherwise walk billions of cells; nothing legitimate
// spans this many tiles in one test.
constexpr int MAX_TILE_SPAN = 512;
constexpr int MAX_RAY_STEPS = 2 * MAX_TILE_SPAN;
constexpr float PARALLEL_EPSILON = 1e-6f;
constexpr float FAR_AWAY = 1e30f;
constexpr float ONE_WAY_FACING = 0.5f;

struct SolidTile
{
    Box box;
    TileCoord coord;
    bool isOneWay = false;
};

SolidTile toSolidTile(const TileGrid& grid, TileCoord coord, const Tileset::TileDefinition& def)
{
    Vec2 min = VEC2_ZERO, max = VEC2_ONE;
    if (def.collision == TileCollision::Custom) {
        min = def.collisionMin;
        max = def.collisionMax;
    }

    Vec2 cell = grid.tileMin(coord.x, coord.y);
    Vec2 worldMin = cell + min * grid.tileSize;
    Vec2 worldMax = cell + max * grid.tileSize;

    return {
        .box =
            {.center = (worldMin + worldMax) * 0.5f, .halfExtents = (worldMax - worldMin) * 0.5f},
        .coord = coord,
        .isOneWay = def.collision == TileCollision::OneWay
    };
}

// `outward` is the tile's face normal toward the mover: a one-way tile resists only motion
// down through its top face, and against any other face is not there at all.
bool oneWayBlocks(const Vec2& outward, const Vec2& motion)
{
    if (outward.y < ONE_WAY_FACING) return false;
    return Vec2::dot(motion, outward) < 0.0f;
}

template<typename Fn>
void forEachSolidTile(
    const TilemapComponent& tilemap, const TileGrid& grid, Vec2 min, Vec2 max, Fn visit
)
{
    if (!grid.isValid()) return;

    TileCoord low = grid.toTile(min);
    TileCoord high = grid.toTile(max);
    if (high.x - low.x > MAX_TILE_SPAN) high.x = low.x + MAX_TILE_SPAN;
    if (high.y - low.y > MAX_TILE_SPAN) high.y = low.y + MAX_TILE_SPAN;

    const TilemapManager& tiles = TilemapManager::get();
    for (int y = low.y; y <= high.y; y++) {
        for (int x = low.x; x <= high.x; x++) {
            const Tileset::TileDefinition* def = tiles.getDefinitionAt(tilemap, x, y);
            if (!def || def->collision == TileCollision::None) continue;
            visit(toSolidTile(grid, {x, y}, *def));
        }
    }
}

// One Contact cannot describe a body wedged against several tiles at once, so the deepest one
// wins -- it is the push that has to happen.
void keepDeepest(Contact& best, const Contact& candidate)
{
    if (!candidate.isTouching) return;
    if (!best.isTouching || candidate.depth > best.depth) best = candidate;
}

void keepNearest(RayHit& best, const RayHit& candidate)
{
    if (!candidate.isHit) return;
    if (!best.isHit || candidate.distance < best.distance) best = candidate;
}

}   // namespace

bool testPointTilemap(const Vec2& point, const TilemapComponent& tilemap, const TileGrid& grid)
{
    if (!grid.isValid()) return false;

    TileCoord coord = grid.toTile(point);
    const Tileset::TileDefinition* def = TilemapManager::get().getDefinitionAt(tilemap, coord);
    if (!def || def->collision == TileCollision::None) return false;

    return testPointBox(point, toSolidTile(grid, coord, *def).box);
}

// Grid traversal rather than the box sweep the casts use: a ray has no thickness, so it can
// walk cell to cell and stop at the first solid one instead of testing a whole region.
RayHit testRayTilemap(const Ray& ray, const TilemapComponent& tilemap, const TileGrid& grid)
{
    if (!grid.isValid()) return {};

    Vec2 worldDelta = ray.end - ray.start;
    float length = worldDelta.magnitude();
    if (length <= 0) return {};

    const TilemapManager& tiles = TilemapManager::get();
    TileCoord tile = grid.toTile(ray.start);

    // A Custom tile does not fill its cell, so traversal only nominates and this decides.
    auto hitTile = [&](TileCoord coord) -> RayHit {
        const Tileset::TileDefinition* def = tiles.getDefinitionAt(tilemap, coord);
        if (!def || def->collision == TileCollision::None) return {};

        SolidTile solid = toSolidTile(grid, coord, *def);
        RayHit hit = testRayBox(ray, solid.box);
        if (!hit.isHit) return {};
        if (solid.isOneWay && !oneWayBlocks(hit.normal, worldDelta)) return {};

        hit.tile = coord;
        return hit;
    };

    if (RayHit hit = hitTile(tile); hit.isHit) return hit;

    // Dividing the tile size out makes every cell a unit square, so the walk below is a plain
    // unit DDA. It steps in the ray's own parameter -- 0 at the start, 1 at the end -- which an
    // affine change of space leaves alone.
    Vec2 localStart = grid.toLocal(ray.start);
    Vec2 localDelta = worldDelta / grid.tileSize;

    int stepX = localDelta.x < 0 ? -1 : 1;
    int stepY = localDelta.y < 0 ? -1 : 1;

    float deltaX =
        Math::abs(localDelta.x) < PARALLEL_EPSILON ? FAR_AWAY : 1.0f / Math::abs(localDelta.x);
    float deltaY =
        Math::abs(localDelta.y) < PARALLEL_EPSILON ? FAR_AWAY : 1.0f / Math::abs(localDelta.y);

    float nextX =
        Math::abs(localDelta.x) < PARALLEL_EPSILON ?
            FAR_AWAY :
            (static_cast<float>(stepX > 0 ? tile.x + 1 : tile.x) - localStart.x) / localDelta.x;
    float nextY =
        Math::abs(localDelta.y) < PARALLEL_EPSILON ?
            FAR_AWAY :
            (static_cast<float>(stepY > 0 ? tile.y + 1 : tile.y) - localStart.y) / localDelta.y;

    for (int step = 0; step < MAX_RAY_STEPS; step++) {
        float t = nextX < nextY ? nextX : nextY;

        if (nextX < nextY) {
            nextX += deltaX;
            tile.x += stepX;
        } else {
            nextY += deltaY;
            tile.y += stepY;
        }

        if (t > 1.0f) return {};
        if (RayHit hit = hitTile(tile); hit.isHit) return hit;
    }
    return {};
}

Contact testBoxTilemap(const Box& box, const TilemapComponent& tilemap, const TileGrid& grid)
{
    Contact deepest;
    forEachSolidTile(
        tilemap, grid, box.center - box.halfExtents, box.center + box.halfExtents,
        [&](const SolidTile& tile) {
            Contact contact = testBoxBox(box, tile.box);
            // No motion to judge a one-way tile by, so the escape direction stands in: it can
            // hold a body up, never shove it sideways or down out of itself.
            if (tile.isOneWay && !oneWayBlocks(-contact.normal, contact.normal)) return;

            contact.tile = tile.coord;
            keepDeepest(deepest, contact);
        }
    );
    return deepest;
}

Contact
testCircleTilemap(const Circle& circle, const TilemapComponent& tilemap, const TileGrid& grid)
{
    Vec2 extent(circle.radius, circle.radius);

    Contact deepest;
    forEachSolidTile(
        tilemap, grid, circle.center - extent, circle.center + extent, [&](const SolidTile& tile) {
            // testBoxCircle runs tile -> circle; the caller asked for circle -> tile.
            Contact contact = testBoxCircle(tile.box, circle);
            contact.normal = -contact.normal;
            if (tile.isOneWay && !oneWayBlocks(-contact.normal, contact.normal)) return;

            contact.tile = tile.coord;
            keepDeepest(deepest, contact);
        }
    );
    return deepest;
}

RayHit testCircleCastTilemap(
    const Ray& path, float radius, const TilemapComponent& tilemap, const TileGrid& grid
)
{
    Vec2 extent(radius, radius);
    Vec2 motion = path.end - path.start;

    RayHit nearest;
    forEachSolidTile(
        tilemap, grid, Vec2::min(path.start, path.end) - extent,
        Vec2::max(path.start, path.end) + extent, [&](const SolidTile& tile) {
            RayHit hit = testCircleCastBox(path, radius, tile.box);
            if (!hit.isHit) return;
            if (tile.isOneWay && !oneWayBlocks(hit.normal, motion)) return;

            hit.tile = tile.coord;
            keepNearest(nearest, hit);
        }
    );
    return nearest;
}

RayHit testBoxCastTilemap(
    const Ray& path, const Vec2& halfExtents, const TilemapComponent& tilemap, const TileGrid& grid
)
{
    Vec2 motion = path.end - path.start;

    RayHit nearest;
    forEachSolidTile(
        tilemap, grid, Vec2::min(path.start, path.end) - halfExtents,
        Vec2::max(path.start, path.end) + halfExtents, [&](const SolidTile& tile) {
            RayHit hit = testBoxCastBox(path, halfExtents, tile.box);
            if (!hit.isHit) return;
            if (tile.isOneWay && !oneWayBlocks(hit.normal, motion)) return;

            hit.tile = tile.coord;
            keepNearest(nearest, hit);
        }
    );
    return nearest;
}

}   // namespace Collisions

}   // namespace Engine
