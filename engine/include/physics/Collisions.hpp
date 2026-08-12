#pragma once

#include "components/ColliderComponent.hpp"
#include "components/RigidBodyComponent.hpp"
#include "components/TilemapComponent.hpp"
#include "components/TransformComponent.hpp"
#include "ecs/registry/Entity.hpp"
#include "ecs/registry/EntityHandle.hpp"
#include "utils/TypeAliases.hpp"
#include "utils/math/Vec2.hpp"

namespace Engine {

namespace Collisions {

struct Box
{
    Vec2 center, halfExtents;
};
struct Circle
{
    Vec2 center;
    float radius;
};
// entity is set by the registry-wide tests, tile additionally by the tilemap ones.
struct Contact
{
    bool isTouching = false;
    Vec2 point, normal;
    float depth;
    Entity entity = NULL_ENTITY;
    TileCoord tile = INVALID_TILE;
};

struct Ray
{
    Vec2 start, end;

    static Ray fromDirection(Vec2 start, Vec2 dir, float length)
    {
        return {start, start + dir.normalized() * length};
    }
};
struct RayHit
{
    bool isHit = false;
    Vec2 point, normal;
    float distance;
    Entity entity = NULL_ENTITY;
    TileCoord tile = INVALID_TILE;
};

struct ResolveSettings
{
    float penetrationSlop = 0.01f;
    float correctionPercent = 0.8f;
    float bounceThreshold = 1.0f;
};

Box toBox(const TransformComponent& t, const ColliderComponent& c);
Circle toCircle(const TransformComponent& t, const ColliderComponent& c);
Box toBox(EntityHandle entity);
Circle toCircle(EntityHandle entity);

bool testPointBox(const Vec2& point, const Box& box);
bool testPointCircle(const Vec2& point, const Circle& circle);

RayHit testRayBox(const Ray& ray, const Box& box);
RayHit testRayCircle(const Ray& ray, const Circle& circle);
RayHit testRayRoundedBox(const Ray& ray, const Box& box, float cornerRadius);

Contact testBoxBox(const Box& b1, const Box& b2);
Contact testCircleCircle(const Circle& c1, const Circle& c2);
Contact testBoxCircle(const Box& b, const Circle& c);

RayHit testCircleCastBox(const Ray& path, float radius, const Box& box);
RayHit testCircleCastCircle(const Ray& path, float radius, const Circle& circle);
RayHit testBoxCastBox(const Ray& path, const Vec2& halfExtents, const Box& box);
RayHit testBoxCastCircle(const Ray& path, const Vec2& halfExtents, const Circle& circle);

bool testPointTilemap(const Vec2& point, const TilemapComponent& tilemap, const TileGrid& grid);
RayHit testRayTilemap(const Ray& ray, const TilemapComponent& tilemap, const TileGrid& grid);
Contact testBoxTilemap(const Box& box, const TilemapComponent& tilemap, const TileGrid& grid);
Contact
testCircleTilemap(const Circle& circle, const TilemapComponent& tilemap, const TileGrid& grid);
RayHit testCircleCastTilemap(
    const Ray& path, float radius, const TilemapComponent& tilemap, const TileGrid& grid
);
RayHit testBoxCastTilemap(
    const Ray& path, const Vec2& halfExtents, const TilemapComponent& tilemap, const TileGrid& grid
);

bool testPointEntity(const Vec2& point, EntityHandle entity);
RayHit testRayEntity(const Ray& ray, EntityHandle entity);
Contact testBoxEntity(const Box& box, EntityHandle entity);
Contact testCircleEntity(const Circle& circle, EntityHandle entity);
RayHit testCircleCastEntity(const Ray& path, float radius, EntityHandle entity);
RayHit testBoxCastEntity(const Ray& path, const Vec2& halfExtents, EntityHandle entity);
RayHit testEntityCastEntity(const Vec2& motion, EntityHandle mover, EntityHandle target);
Contact testEntities(EntityHandle a, EntityHandle b);

PhysicsSurfaceOptions surfaceOf(EntityHandle entity, TileCoord tile = INVALID_TILE);

void correctPositions(
    EntityHandle a, EntityHandle b, const Contact& contact, const ResolveSettings& settings = {}
);
void applyImpulse(
    EntityHandle a, EntityHandle b, const Contact& contact, const ResolveSettings& settings = {}
);
void resolveContact(
    EntityHandle a, EntityHandle b, const Contact& contact, const ResolveSettings& settings = {}
);

}   // namespace Collisions

}   // namespace Engine
