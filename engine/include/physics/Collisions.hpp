#pragma once

#include "components/ColliderComponent.hpp"
#include "components/RigidBodyComponent.hpp"
#include "components/TransformComponent.hpp"
#include "ecs/registry/Entity.hpp"
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
struct Contact
{
    bool isTouching = false;
    Vec2 point, normal;
    float depth;
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
    Entity entity = NULL_ENTITY;   // only set if testing against whole registery
};

struct ResolveSettings
{
    float penetrationSlop = 0.01f;
    float correctionPercent = 0.8f;
};

Box toBox(const TransformComponent& t, const ColliderComponent& c);
Circle toCircle(const TransformComponent& t, const ColliderComponent& c);

bool testPointBox(const Vec2& point, const Box& box);
bool testPointCircle(const Vec2& point, const Circle& circle);
bool testPointCollider(const Vec2& point, const TransformComponent& t, const ColliderComponent& c);

RayHit testRayBox(const Ray& ray, const Box& box);
RayHit testRayCircle(const Ray& ray, const Circle& circle);
RayHit testRayCollider(const Ray& ray, const TransformComponent& t, const ColliderComponent& c);

Contact testBoxBox(const Box& b1, const Box& b2);
Contact testCircleCircle(const Circle& c1, const Circle& c2);
Contact testBoxCircle(const Box& b, const Circle& c);
Contact testColliders(
    const TransformComponent& t1, const ColliderComponent& c1, const TransformComponent& t2,
    const ColliderComponent& c2
);

void resolveContact(
    TransformComponent& t1, RigidBodyComponent* body1, TransformComponent& t2,
    RigidBodyComponent* body2, const Contact& contact, const ResolveSettings& settings = {}
);

}   // namespace Collisions

}   // namespace Engine
