#include "graphics/tilemap/TilemapManager.hpp"
#include "physics/Collisions.hpp"
#include "utils/math/MathFuncs.hpp"

namespace Engine {

namespace Collisions {

namespace {

// A query the size of the world would otherwise walk billions of cells; nothing legitimate
// spans this many tiles in one test.
constexpr int MAX_TILE_SPAN = 512;
constexpr float PARALLEL_EPSILON = 1e-6f;
constexpr float FAR_AWAY = 1e30f;

int tileCoord(float world) { return static_cast<int>(Math::floor(world)); }

Box tileBox(int x, int y)
{
    return {
        .center = Vec2(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f),
        .halfExtents = Vec2(0.5f, 0.5f)
    };
}

template<typename Fn>
void forEachSolidTile(const TilemapComponent& tilemap, Vec2 min, Vec2 max, Fn visit)
{
    int minX = tileCoord(min.x), minY = tileCoord(min.y);
    int maxX = tileCoord(max.x), maxY = tileCoord(max.y);
    if (maxX - minX > MAX_TILE_SPAN) maxX = minX + MAX_TILE_SPAN;
    if (maxY - minY > MAX_TILE_SPAN) maxY = minY + MAX_TILE_SPAN;

    for (int y = minY; y <= maxY; y++) {
        for (int x = minX; x <= maxX; x++) {
            if (TilemapManager::get().isSolidAt(tilemap, x, y)) visit(tileBox(x, y));
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

bool testPointTilemap(const Vec2& point, const TilemapComponent& tilemap)
{
    return TilemapManager::get().isSolidAt(tilemap, tileCoord(point.x), tileCoord(point.y));
}

// Grid traversal rather than the box sweep the casts use: a ray has no thickness, so it can
// walk cell to cell and stop at the first solid one instead of testing a whole region.
RayHit testRayTilemap(const Ray& ray, const TilemapComponent& tilemap)
{
    Vec2 delta = ray.end - ray.start;
    float length = delta.magnitude();
    if (length <= 0) return {};
    Vec2 direction = delta / length;

    int x = tileCoord(ray.start.x), y = tileCoord(ray.start.y);

    if (TilemapManager::get().isSolidAt(tilemap, x, y)) {
        RayHit hit;
        hit.isHit = true;
        hit.distance = 0.0f;
        hit.point = ray.start;
        hit.normal = VEC2_ZERO;
        return hit;
    }

    int stepX = direction.x < 0 ? -1 : 1;
    int stepY = direction.y < 0 ? -1 : 1;

    // Distance along the ray between successive grid lines, and to the first one.
    float deltaX =
        Math::abs(direction.x) < PARALLEL_EPSILON ? FAR_AWAY : 1.0f / Math::abs(direction.x);
    float deltaY =
        Math::abs(direction.y) < PARALLEL_EPSILON ? FAR_AWAY : 1.0f / Math::abs(direction.y);

    float nextX = Math::abs(direction.x) < PARALLEL_EPSILON ?
                      FAR_AWAY :
                      (static_cast<float>(stepX > 0 ? x + 1 : x) - ray.start.x) / direction.x;
    float nextY = Math::abs(direction.y) < PARALLEL_EPSILON ?
                      FAR_AWAY :
                      (static_cast<float>(stepY > 0 ? y + 1 : y) - ray.start.y) / direction.y;

    while (true) {
        float distance;
        Vec2 normal;

        if (nextX < nextY) {
            distance = nextX;
            nextX += deltaX;
            x += stepX;
            normal = Vec2(static_cast<float>(-stepX), 0.0f);
        } else {
            distance = nextY;
            nextY += deltaY;
            y += stepY;
            normal = Vec2(0.0f, static_cast<float>(-stepY));
        }

        if (distance > length) return {};
        if (!TilemapManager::get().isSolidAt(tilemap, x, y)) continue;

        RayHit hit;
        hit.isHit = true;
        hit.distance = distance;
        hit.point = ray.start + direction * distance;
        hit.normal = normal;
        return hit;
    }
}

Contact testBoxTilemap(const Box& box, const TilemapComponent& tilemap)
{
    Contact deepest;
    forEachSolidTile(
        tilemap, box.center - box.halfExtents, box.center + box.halfExtents,
        [&](const Box& tile) { keepDeepest(deepest, testBoxBox(box, tile)); }
    );
    return deepest;
}

Contact testCircleTilemap(const Circle& circle, const TilemapComponent& tilemap)
{
    Vec2 extent(circle.radius, circle.radius);

    Contact deepest;
    forEachSolidTile(tilemap, circle.center - extent, circle.center + extent, [&](const Box& tile) {
        // testBoxCircle runs tile -> circle; the caller asked for circle -> tile.
        Contact contact = testBoxCircle(tile, circle);
        contact.normal = -contact.normal;
        keepDeepest(deepest, contact);
    });
    return deepest;
}

RayHit testCircleCastTilemap(const Ray& path, float radius, const TilemapComponent& tilemap)
{
    Vec2 extent(radius, radius);

    RayHit nearest;
    forEachSolidTile(
        tilemap, Vec2::min(path.start, path.end) - extent, Vec2::max(path.start, path.end) + extent,
        [&](const Box& tile) { keepNearest(nearest, testCircleCastBox(path, radius, tile)); }
    );
    return nearest;
}

RayHit testBoxCastTilemap(const Ray& path, const Vec2& halfExtents, const TilemapComponent& tilemap)
{
    RayHit nearest;
    forEachSolidTile(
        tilemap, Vec2::min(path.start, path.end) - halfExtents,
        Vec2::max(path.start, path.end) + halfExtents,
        [&](const Box& tile) { keepNearest(nearest, testBoxCastBox(path, halfExtents, tile)); }
    );
    return nearest;
}

}   // namespace Collisions

}   // namespace Engine
