#include "physics/Collisions.hpp"
#include "utils/math/MathFuncs.hpp"

namespace Engine {

namespace Collisions {

Box toBox(const TransformComponent& t, const ColliderComponent& c)
{
    Vec2 scale = t.scale.abs();
    Vec2 center = t.position;
    return {.center = center + c.offset * scale, .halfExtents = c.halfExtents * scale};
}
// A circle cannot follow a non-uniform scale without becoming an ellipse, so the larger axis
// wins and the collider stays a circle.
Circle toCircle(const TransformComponent& t, const ColliderComponent& c)
{
    Vec2 scale = t.scale.abs();
    Vec2 center = t.position;
    return {.center = center + c.offset * scale, .radius = c.radius * Math::max(scale.x, scale.y)};
}

Contact testBoxBox(const Box& b1, const Box& b2)
{
    Vec2 delta = b2.center - b1.center;
    Vec2 overlap = b1.halfExtents + b2.halfExtents - delta.abs();
    if (overlap.x <= 0 || overlap.y <= 0) return {};

    Contact contact;
    contact.isTouching = true;

    // The axis of least overlap is the cheapest way out, and picking the other one would drag
    // a body along a wall it is only grazing.
    if (overlap.x < overlap.y) {
        contact.normal = Vec2(delta.x < 0 ? -1.0f : 1.0f, 0.0f);
        contact.depth = overlap.x;
    } else {
        contact.normal = Vec2(0.0f, delta.y < 0 ? -1.0f : 1.0f);
        contact.depth = overlap.y;
    }

    Vec2 overlapMin = Vec2::max(b1.center - b1.halfExtents, b2.center - b2.halfExtents);
    Vec2 overlapMax = Vec2::min(b1.center + b1.halfExtents, b2.center + b2.halfExtents);
    contact.point = (overlapMin + overlapMax) * 0.5f;

    return contact;
}

Contact testCircleCircle(const Circle& c1, const Circle& c2)
{
    Vec2 delta = c2.center - c1.center;
    float radiusSum = c1.radius + c2.radius;
    float sqrDistance = delta.sqrMagnitude();
    if (sqrDistance >= radiusSum * radiusSum) return {};

    Contact contact;
    contact.isTouching = true;

    float distance = Math::sqrt(sqrDistance);
    // Concentric circles have no meaningful direction to separate along; any axis is as good
    // as another and picking one keeps the normal unit length instead of NaN.
    contact.normal = distance > 0 ? delta / distance : VEC2_RIGHT;
    contact.depth = radiusSum - distance;
    contact.point = c1.center + contact.normal * (c1.radius - contact.depth * 0.5f);

    return contact;
}

Contact testBoxCircle(const Box& b, const Circle& c)
{
    Vec2 boxMin = b.center - b.halfExtents;
    Vec2 boxMax = b.center + b.halfExtents;
    Vec2 closest = Vec2::max(boxMin, Vec2::min(c.center, boxMax));
    Vec2 delta = c.center - closest;
    float sqrDistance = delta.sqrMagnitude();

    Contact contact;

    // Clamping gives back the circle's own centre once it is inside the box, so there is no
    // direction left to read off it -- fall back to the nearest face, as box/box does.
    if (sqrDistance <= 0) {
        Vec2 fromCenter = c.center - b.center;
        Vec2 toFace = b.halfExtents - fromCenter.abs();

        contact.isTouching = true;
        if (toFace.x < toFace.y) {
            contact.normal = Vec2(fromCenter.x < 0 ? -1.0f : 1.0f, 0.0f);
            contact.depth = toFace.x + c.radius;
        } else {
            contact.normal = Vec2(0.0f, fromCenter.y < 0 ? -1.0f : 1.0f);
            contact.depth = toFace.y + c.radius;
        }
        contact.point = c.center;
        return contact;
    }

    if (sqrDistance >= c.radius * c.radius) return {};

    float distance = Math::sqrt(sqrDistance);
    contact.isTouching = true;
    contact.normal = delta / distance;
    contact.depth = c.radius - distance;
    contact.point = closest;

    return contact;
}

// The only place that knows box/circle is asymmetric: a circle-first pair is tested flipped
// and the normal turned back around, so every caller reads it as running c1 -> c2.
Contact testColliders(
    const TransformComponent& t1, const ColliderComponent& c1, const TransformComponent& t2,
    const ColliderComponent& c2
)
{
    if (c1.shape == ColliderShape::Tilemap || c2.shape == ColliderShape::Tilemap) return {};

    if (c1.shape == ColliderShape::Box && c2.shape == ColliderShape::Box) {
        return testBoxBox(toBox(t1, c1), toBox(t2, c2));
    }
    if (c1.shape == ColliderShape::Circle && c2.shape == ColliderShape::Circle) {
        return testCircleCircle(toCircle(t1, c1), toCircle(t2, c2));
    }
    if (c1.shape == ColliderShape::Box) { return testBoxCircle(toBox(t1, c1), toCircle(t2, c2)); }

    Contact contact = testBoxCircle(toBox(t2, c2), toCircle(t1, c1));
    contact.normal = -contact.normal;
    return contact;
}

}   // namespace Collisions

}   // namespace Engine
