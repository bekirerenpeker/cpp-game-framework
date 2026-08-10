#include "physics/Collisions.hpp"
#include "utils/math/MathFuncs.hpp"

namespace Engine {

namespace Collisions {

namespace {

// Below this a slab is treated as parallel to the ray rather than divided by, which would give
// an infinity that turns into a NaN wherever the numerator is also zero.
constexpr float PARALLEL_EPSILON = 1e-6f;

}   // namespace

bool testPointBox(const Vec2& point, const Box& box)
{
    return point.x > box.center.x - box.halfExtents.x &&
           point.x < box.center.x + box.halfExtents.x &&
           point.y > box.center.y - box.halfExtents.y && point.y < box.center.y + box.halfExtents.y;
}
bool testPointCircle(const Vec2& point, const Circle& circle)
{
    return (point - circle.center).sqrMagnitude() <= circle.radius * circle.radius;
}
bool testPointCollider(const Vec2& point, const TransformComponent& t, const ColliderComponent& c)
{
    switch (c.shape) {
    case ColliderShape::Box    : return testPointBox(point, toBox(t, c));
    case ColliderShape::Circle : return testPointCircle(point, toCircle(t, c));
    case ColliderShape::Tilemap:
    default                    : return false;
    }
}

// Slab method: clip the ray against each axis' pair of faces in turn and keep the latest entry
// and earliest exit. They cross over each other the moment the ray misses.
RayHit testRayBox(const Ray& ray, const Box& box)
{
    Vec2 delta = ray.end - ray.start;
    float length = delta.magnitude();
    if (length <= 0) return {};
    Vec2 direction = delta / length;

    Vec2 boxMin = box.center - box.halfExtents;
    Vec2 boxMax = box.center + box.halfExtents;

    const float origin[2] = {ray.start.x, ray.start.y};
    const float dir[2] = {direction.x, direction.y};
    const float low[2] = {boxMin.x, boxMin.y};
    const float high[2] = {boxMax.x, boxMax.y};

    float tEnter = 0.0f, tExit = length;
    Vec2 enterNormal = VEC2_ZERO;

    for (int axis = 0; axis < 2; axis++) {
        if (Math::abs(dir[axis]) < PARALLEL_EPSILON) {
            if (origin[axis] < low[axis] || origin[axis] > high[axis]) return {};
            continue;
        }

        float tLow = (low[axis] - origin[axis]) / dir[axis];
        float tHigh = (high[axis] - origin[axis]) / dir[axis];

        // Travelling in -axis means the far face is hit first, and the entry normal flips.
        float normalSign = -1.0f;
        if (tLow > tHigh) {
            float swap = tLow;
            tLow = tHigh;
            tHigh = swap;
            normalSign = 1.0f;
        }

        if (tLow > tEnter) {
            tEnter = tLow;
            enterNormal = axis == 0 ? Vec2(normalSign, 0.0f) : Vec2(0.0f, normalSign);
        }
        if (tHigh < tExit) tExit = tHigh;
        if (tEnter > tExit) return {};
    }

    RayHit hit;
    hit.isHit = true;
    hit.distance = tEnter;
    hit.point = ray.start + direction * tEnter;
    // A ray starting inside never crosses a face, so it reports a zero normal at zero distance.
    hit.normal = enterNormal;

    return hit;
}

RayHit testRayCircle(const Ray& ray, const Circle& circle)
{
    Vec2 delta = ray.end - ray.start;
    float length = delta.magnitude();
    if (length <= 0) return {};
    Vec2 direction = delta / length;

    Vec2 toStart = ray.start - circle.center;
    float projection = Vec2::dot(toStart, direction);
    float gap = toStart.sqrMagnitude() - circle.radius * circle.radius;
    if (gap > 0 && projection > 0) return {};

    float discriminant = projection * projection - gap;
    if (discriminant < 0) return {};

    float entry = -projection - Math::sqrt(discriminant);
    if (entry > length) return {};
    if (entry < 0) entry = 0;

    RayHit hit;
    hit.isHit = true;
    hit.distance = entry;
    hit.point = ray.start + direction * entry;
    hit.normal = (hit.point - circle.center).normalized();

    return hit;
}

RayHit testRayCollider(const Ray& ray, const TransformComponent& t, const ColliderComponent& c)
{
    switch (c.shape) {
    case ColliderShape::Box    : return testRayBox(ray, toBox(t, c));
    case ColliderShape::Circle : return testRayCircle(ray, toCircle(t, c));
    case ColliderShape::Tilemap:
    default                    : return {};
    }
}

}   // namespace Collisions

}   // namespace Engine
