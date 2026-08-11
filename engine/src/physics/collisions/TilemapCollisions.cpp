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

Box tileBox(const TileGrid& grid, int x, int y)
{
    Vec2 halfExtents = grid.tileSize * 0.5f;
    return {.center = grid.tileMin(x, y) + halfExtents, .halfExtents = halfExtents};
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

    for (int y = low.y; y <= high.y; y++) {
        for (int x = low.x; x <= high.x; x++) {
            if (TilemapManager::get().isSolidAt(tilemap, x, y)) visit(tileBox(grid, x, y));
        }
    }
}

// One Contact cannot describe a body wedged against several tiles at once, so the deepest one
// wins -- it is the push that has to happen. Axis-separated movement is what actually fixes
// this, and belongs in the mover rather than here.
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
    return TilemapManager::get().isSolidAt(tilemap, grid.toTile(point));
}

// Grid traversal rather than the box sweep the casts use: a ray has no thickness, so it can
// walk cell to cell and stop at the first solid one instead of testing a whole region.
RayHit testRayTilemap(const Ray& ray, const TilemapComponent& tilemap, const TileGrid& grid)
{
    if (!grid.isValid()) return {};

    Vec2 worldDelta = ray.end - ray.start;
    float length = worldDelta.magnitude();
    if (length <= 0) return {};

    TileCoord tile = grid.toTile(ray.start);
    if (TilemapManager::get().isSolidAt(tilemap, tile)) {
        RayHit hit;
        hit.isHit = true;
        hit.distance = 0.0f;
        hit.point = ray.start;
        hit.normal = VEC2_ZERO;
        return hit;
    }

    // Dividing the tile size out makes every cell a unit square, so the walk below is a plain
    // unit DDA. It steps in the ray's own parameter -- 0 at the start, 1 at the end -- which an
    // affine change of space leaves alone, so the hit converts back with one lerp.
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

    // A tile far smaller than the ray is long would otherwise walk millions of cells.
    for (int step = 0; step < MAX_RAY_STEPS; step++) {
        float t;
        Vec2 normal;

        if (nextX < nextY) {
            t = nextX;
            nextX += deltaX;
            tile.x += stepX;
            normal = Vec2(static_cast<float>(-stepX), 0.0f);
        } else {
            t = nextY;
            nextY += deltaY;
            tile.y += stepY;
            normal = Vec2(0.0f, static_cast<float>(-stepY));
        }

        if (t > 1.0f) return {};
        if (!TilemapManager::get().isSolidAt(tilemap, tile)) continue;

        RayHit hit;
        hit.isHit = true;
        hit.distance = t * length;
        hit.point = ray.start + worldDelta * t;
        hit.normal = normal;
        return hit;
    }
    return {};
}

Contact testBoxTilemap(const Box& box, const TilemapComponent& tilemap, const TileGrid& grid)
{
    Contact deepest;
    forEachSolidTile(
        tilemap, grid, box.center - box.halfExtents, box.center + box.halfExtents,
        [&](const Box& tile) { keepDeepest(deepest, testBoxBox(box, tile)); }
    );
    return deepest;
}

Contact
testCircleTilemap(const Circle& circle, const TilemapComponent& tilemap, const TileGrid& grid)
{
    Vec2 extent(circle.radius, circle.radius);

    Contact deepest;
    forEachSolidTile(
        tilemap, grid, circle.center - extent, circle.center + extent, [&](const Box& tile) {
            // testBoxCircle runs tile -> circle; the caller asked for circle -> tile.
            Contact contact = testBoxCircle(tile, circle);
            contact.normal = -contact.normal;
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

    RayHit nearest;
    forEachSolidTile(
        tilemap, grid, Vec2::min(path.start, path.end) - extent,
        Vec2::max(path.start, path.end) + extent,
        [&](const Box& tile) { keepNearest(nearest, testCircleCastBox(path, radius, tile)); }
    );
    return nearest;
}

RayHit testBoxCastTilemap(
    const Ray& path, const Vec2& halfExtents, const TilemapComponent& tilemap, const TileGrid& grid
)
{
    RayHit nearest;
    forEachSolidTile(
        tilemap, grid, Vec2::min(path.start, path.end) - halfExtents,
        Vec2::max(path.start, path.end) + halfExtents,
        [&](const Box& tile) { keepNearest(nearest, testBoxCastBox(path, halfExtents, tile)); }
    );
    return nearest;
}

}   // namespace Collisions

}   // namespace Engine
