#include "physics/Collisions.hpp"

namespace Engine {

namespace Collisions {

namespace {

// Every entity-facing test asks the same three questions -- which shape is this, can it be
// built, and what runs against it -- so they all funnel through here and differ only in the
// three callbacks. A missing component answers "no contact" rather than asserting: an entity
// may hold a collider before it holds anything else.
template<typename R, typename BoxFn, typename CircleFn, typename TilemapFn>
R dispatch(EntityHandle entity, BoxFn onBox, CircleFn onCircle, TilemapFn onTilemap)
{
    ColliderComponent* collider = entity.tryGet<ColliderComponent>();
    if (!collider) return R {};

    TransformComponent* transform = entity.tryGet<TransformComponent>();
    if (!transform) return R {};

    if (collider->shape == ColliderShape::Tilemap) {
        TilemapComponent* tilemap = entity.tryGet<TilemapComponent>();
        return tilemap ? onTilemap(*tilemap, TileGrid::fromTransform(*transform)) : R {};
    }

    if (collider->shape == ColliderShape::Circle) return onCircle(toCircle(*transform, *collider));
    return onBox(toBox(*transform, *collider));
}

bool isTilemap(EntityHandle entity)
{
    ColliderComponent* collider = entity.tryGet<ColliderComponent>();
    return collider && collider->shape == ColliderShape::Tilemap;
}

}   // namespace

Box toBox(EntityHandle entity)
{
    TransformComponent* transform = entity.tryGet<TransformComponent>();
    ColliderComponent* collider = entity.tryGet<ColliderComponent>();
    if (!transform || !collider) return {};
    return toBox(*transform, *collider);
}

Circle toCircle(EntityHandle entity)
{
    TransformComponent* transform = entity.tryGet<TransformComponent>();
    ColliderComponent* collider = entity.tryGet<ColliderComponent>();
    if (!transform || !collider) return {};
    return toCircle(*transform, *collider);
}

bool testPointEntity(const Vec2& point, EntityHandle entity)
{
    return dispatch<bool>(
        entity,   //
        [&](const Box& box) { return testPointBox(point, box); },
        [&](const Circle& circle) { return testPointCircle(point, circle); },
        [&](const TilemapComponent& tilemap, const TileGrid& grid) {
            return testPointTilemap(point, tilemap, grid);
        }
    );
}

RayHit testRayEntity(const Ray& ray, EntityHandle entity)
{
    return dispatch<RayHit>(
        entity,   //
        [&](const Box& box) { return testRayBox(ray, box); },
        [&](const Circle& circle) { return testRayCircle(ray, circle); },
        [&](const TilemapComponent& tilemap, const TileGrid& grid) {
            return testRayTilemap(ray, tilemap, grid);
        }
    );
}

Contact testBoxEntity(const Box& box, EntityHandle entity)
{
    return dispatch<Contact>(
        entity,   //
        [&](const Box& other) { return testBoxBox(box, other); },
        [&](const Circle& other) { return testBoxCircle(box, other); },
        [&](const TilemapComponent& tilemap, const TileGrid& grid) {
            return testBoxTilemap(box, tilemap, grid);
        }
    );
}

Contact testCircleEntity(const Circle& circle, EntityHandle entity)
{
    return dispatch<Contact>(
        entity,
        // Only box/circle is asymmetric, so this is the one place a normal is turned around.
        [&](const Box& other) {
            Contact contact = testBoxCircle(other, circle);
            contact.normal = -contact.normal;
            return contact;
        },
        [&](const Circle& other) { return testCircleCircle(circle, other); },
        [&](const TilemapComponent& tilemap, const TileGrid& grid) {
            return testCircleTilemap(circle, tilemap, grid);
        }
    );
}

RayHit testCircleCastEntity(const Ray& path, float radius, EntityHandle entity)
{
    return dispatch<RayHit>(
        entity,   //
        [&](const Box& box) { return testCircleCastBox(path, radius, box); },
        [&](const Circle& circle) { return testCircleCastCircle(path, radius, circle); },
        [&](const TilemapComponent& tilemap, const TileGrid& grid) {
            return testCircleCastTilemap(path, radius, tilemap, grid);
        }
    );
}

RayHit testBoxCastEntity(const Ray& path, const Vec2& halfExtents, EntityHandle entity)
{
    return dispatch<RayHit>(
        entity,   //
        [&](const Box& box) { return testBoxCastBox(path, halfExtents, box); },
        [&](const Circle& circle) { return testBoxCastCircle(path, halfExtents, circle); },
        [&](const TilemapComponent& tilemap, const TileGrid& grid) {
            return testBoxCastTilemap(path, halfExtents, tilemap, grid);
        }
    );
}

RayHit testEntityCastEntity(const Vec2& motion, EntityHandle mover, EntityHandle target)
{
    return dispatch<RayHit>(
        mover,
        [&](const Box& box) {
            return testBoxCastEntity({box.center, box.center + motion}, box.halfExtents, target);
        },
        [&](const Circle& circle) {
            return testCircleCastEntity(
                {circle.center, circle.center + motion}, circle.radius, target
            );
        },
        [&](const TilemapComponent&, const TileGrid&) { return RayHit {}; }
    );
}

Contact testEntities(EntityHandle a, EntityHandle b)
{
    return dispatch<Contact>(
        a,   //
        [&](const Box& box) { return testBoxEntity(box, b); },
        [&](const Circle& circle) { return testCircleEntity(circle, b); },
        [&](const TilemapComponent&, const TileGrid&) {
            // A tilemap is only ever the thing being tested against, so swap the pair and turn
            // the normal back around. Two tilemaps have nothing to say to each other.
            if (isTilemap(b)) return Contact {};
            Contact contact = testEntities(b, a);
            contact.normal = -contact.normal;
            return contact;
        }
    );
}

}   // namespace Collisions

}   // namespace Engine
