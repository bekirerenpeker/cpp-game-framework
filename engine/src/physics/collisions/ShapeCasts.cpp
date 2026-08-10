#include "physics/Collisions.hpp"

namespace Engine {

namespace Collisions {

// Sweeping a shape along a path is the same problem as casting a ray at the target grown by that
// shape -- the Minkowski sum -- so nothing here steps anything, it grows a shape and reuses the
// ray tests. Two boxes sum to a plain box; anything involving a circle sums to a box with rounded
// corners, and the rounding is what stops a sweep catching on empty space diagonally out from a
// corner. The ray then lands where the swept shape's *centre* stops, so each cast walks that
// back onto the target's surface before returning it as the contact point.

RayHit testRayRoundedBox(const Ray& ray, const Box& box, float cornerRadius)
{
    if (cornerRadius <= 0) return testRayBox(ray, box);

    Box outer {
        .center = box.center, .halfExtents = box.halfExtents + Vec2(cornerRadius, cornerRadius)
    };
    RayHit hit = testRayBox(ray, outer);
    if (!hit.isHit) return hit;

    Vec2 fromCenter = hit.point - box.center;
    Vec2 beyond = fromCenter.abs() - box.halfExtents;
    // Entering the outer box anywhere but a corner square lands on a flat face, where the slab
    // result is already the answer. Out of a corner square the only way in is through the arc.
    if (beyond.x <= 0 || beyond.y <= 0) return hit;

    Circle corner {
        .center = box.center + Vec2(
                                   fromCenter.x < 0 ? -box.halfExtents.x : box.halfExtents.x,
                                   fromCenter.y < 0 ? -box.halfExtents.y : box.halfExtents.y
                               ),
        .radius = cornerRadius
    };
    return testRayCircle(ray, corner);
}

RayHit testCircleCastBox(const Ray& path, float radius, const Box& box)
{
    RayHit hit = testRayRoundedBox(path, box, radius);
    if (hit.isHit) hit.point -= hit.normal * radius;
    return hit;
}

RayHit testCircleCastCircle(const Ray& path, float radius, const Circle& circle)
{
    RayHit hit = testRayCircle(path, {.center = circle.center, .radius = circle.radius + radius});
    if (hit.isHit) hit.point -= hit.normal * radius;
    return hit;
}

RayHit testCircleCastCollider(
    const Ray& path, float radius, const TransformComponent& t, const ColliderComponent& c
)
{
    switch (c.shape) {
    case ColliderShape::Box   : return testCircleCastBox(path, radius, toBox(t, c));
    case ColliderShape::Circle: return testCircleCastCircle(path, radius, toCircle(t, c));
    default                   : return {};
    }
}

RayHit testBoxCastBox(const Ray& path, const Vec2& halfExtents, const Box& box)
{
    RayHit hit =
        testRayBox(path, {.center = box.center, .halfExtents = box.halfExtents + halfExtents});
    if (!hit.isHit) return hit;

    hit.point =
        Vec2::max(box.center - box.halfExtents, Vec2::min(hit.point, box.center + box.halfExtents));
    return hit;
}

// The roles swap here: the sum is a box the size of the *casting* box, sitting on the target
// circle's centre and rounded by the target's radius.
RayHit testBoxCastCircle(const Ray& path, const Vec2& halfExtents, const Circle& circle)
{
    RayHit hit = testRayRoundedBox(
        path, {.center = circle.center, .halfExtents = halfExtents}, circle.radius
    );
    if (hit.isHit) hit.point = circle.center + hit.normal * circle.radius;
    return hit;
}

RayHit testBoxCastCollider(
    const Ray& path, const Vec2& halfExtents, const TransformComponent& t,
    const ColliderComponent& c
)
{
    switch (c.shape) {
    case ColliderShape::Box   : return testBoxCastBox(path, halfExtents, toBox(t, c));
    case ColliderShape::Circle: return testBoxCastCircle(path, halfExtents, toCircle(t, c));
    default                   : return {};
    }
}

}   // namespace Collisions

}   // namespace Engine
