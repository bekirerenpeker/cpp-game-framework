#include "components/TransformComponent.hpp"
#include "ecs/registry/View.hpp"
#include "physics/Collisions.hpp"
#include "physics/PhysicsManager.hpp"
#include <algorithm>

namespace Engine {

namespace {

template<typename TestFn>
std::vector<Collisions::RayHit> gatherHits(Registry& registry, TestFn test)
{
    std::vector<Collisions::RayHit> hits;
    View<TransformComponent, ColliderComponent> colliderView(registry);
    colliderView.each([&](Entity entity, TransformComponent& t, ColliderComponent& c) {
        Collisions::RayHit hit = test(t, c);
        if (!hit.isHit) return;
        hit.entity = entity;
        hits.push_back(hit);
    });

    std::sort(
        hits.begin(), hits.end(), [](const Collisions::RayHit& a, const Collisions::RayHit& b) {
            return a.distance < b.distance;
        }
    );
    return hits;
}

template<typename TestFn> Collisions::RayHit closestHit(Registry& registry, TestFn test)
{
    Collisions::RayHit closest;
    View<TransformComponent, ColliderComponent> colliderView(registry);
    colliderView.each([&](Entity entity, TransformComponent& t, ColliderComponent& c) {
        Collisions::RayHit hit = test(t, c);
        if (!hit.isHit) return;
        if (closest.isHit && hit.distance >= closest.distance) return;

        hit.entity = entity;
        closest = hit;
    });
    return closest;
}

}   // namespace

std::vector<Entity> PhysicsManager::pointTest(Registry& registry, const Vec2& point)
{
    std::vector<Entity> entities;
    View<TransformComponent, ColliderComponent> colliderView(registry);
    colliderView.each([&](Entity entity, TransformComponent& t, ColliderComponent& c) {
        if (Collisions::testPointCollider(point, t, c)) entities.push_back(entity);
    });
    return entities;
}

std::vector<Collisions::Contact>
PhysicsManager::circleTest(Registry& registry, const Collisions::Circle& circle)
{
    std::vector<Collisions::Contact> contacts;
    View<TransformComponent, ColliderComponent> colliderView(registry);
    colliderView.each([&](Entity entity, TransformComponent& t, ColliderComponent& c) {
        Collisions::Contact contact = Collisions::testCircleCollider(circle, t, c);
        contact.entity = entity;
        if (contact.isTouching) contacts.push_back(contact);
    });
    return contacts;
}

std::vector<Collisions::Contact>
PhysicsManager::boxTest(Registry& registry, const Collisions::Box& box)
{
    std::vector<Collisions::Contact> contacts;
    View<TransformComponent, ColliderComponent> colliderView(registry);
    colliderView.each([&](Entity entity, TransformComponent& t, ColliderComponent& c) {
        Collisions::Contact contact = Collisions::testBoxCollider(box, t, c);
        contact.entity = entity;
        if (contact.isTouching) contacts.push_back(contact);
    });
    return contacts;
}

Collisions::RayHit PhysicsManager::rayTest(Registry& registry, const Collisions::Ray& ray)
{
    return closestHit(registry, [&](TransformComponent& t, ColliderComponent& c) {
        return Collisions::testRayCollider(ray, t, c);
    });
}

std::vector<Collisions::RayHit>
PhysicsManager::rayTestAll(Registry& registry, const Collisions::Ray& ray)
{
    return gatherHits(registry, [&](TransformComponent& t, ColliderComponent& c) {
        return Collisions::testRayCollider(ray, t, c);
    });
}

Collisions::RayHit
PhysicsManager::circleCast(Registry& registry, const Collisions::Ray& path, float radius)
{
    return closestHit(registry, [&](TransformComponent& t, ColliderComponent& c) {
        return Collisions::testCircleCastCollider(path, radius, t, c);
    });
}

std::vector<Collisions::RayHit>
PhysicsManager::circleCastAll(Registry& registry, const Collisions::Ray& path, float radius)
{
    return gatherHits(registry, [&](TransformComponent& t, ColliderComponent& c) {
        return Collisions::testCircleCastCollider(path, radius, t, c);
    });
}

Collisions::RayHit
PhysicsManager::boxCast(Registry& registry, const Collisions::Ray& path, const Vec2& halfExtents)
{
    return closestHit(registry, [&](TransformComponent& t, ColliderComponent& c) {
        return Collisions::testBoxCastCollider(path, halfExtents, t, c);
    });
}

std::vector<Collisions::RayHit>
PhysicsManager::boxCastAll(Registry& registry, const Collisions::Ray& path, const Vec2& halfExtents)
{
    return gatherHits(registry, [&](TransformComponent& t, ColliderComponent& c) {
        return Collisions::testBoxCastCollider(path, halfExtents, t, c);
    });
}

}   // namespace Engine
