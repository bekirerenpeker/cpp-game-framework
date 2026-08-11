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
    View<ColliderComponent> colliderView(registry);
    colliderView.each([&](Entity entity, ColliderComponent&) {
        Collisions::RayHit hit = test(EntityHandle(entity, registry));
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
    View<ColliderComponent> colliderView(registry);
    colliderView.each([&](Entity entity, ColliderComponent&) {
        Collisions::RayHit hit = test(EntityHandle(entity, registry));
        if (!hit.isHit) return;
        if (closest.isHit && hit.distance >= closest.distance) return;

        hit.entity = entity;
        closest = hit;
    });
    return closest;
}

template<typename TestFn>
std::vector<Collisions::Contact> gatherContacts(Registry& registry, TestFn test)
{
    std::vector<Collisions::Contact> contacts;
    View<ColliderComponent> colliderView(registry);
    colliderView.each([&](Entity entity, ColliderComponent&) {
        Collisions::Contact contact = test(EntityHandle(entity, registry));
        if (!contact.isTouching) return;

        contact.entity = entity;
        contacts.push_back(contact);
    });
    return contacts;
}

}   // namespace

std::vector<Entity> PhysicsManager::pointTest(Registry& registry, const Vec2& point)
{
    std::vector<Entity> entities;
    View<ColliderComponent> colliderView(registry);
    colliderView.each([&](Entity entity, ColliderComponent&) {
        if (Collisions::testPointEntity(point, EntityHandle(entity, registry))) {
            entities.push_back(entity);
        }
    });
    return entities;
}

std::vector<Collisions::Contact>
PhysicsManager::circleTest(Registry& registry, const Collisions::Circle& circle)
{
    return gatherContacts(registry, [&](EntityHandle entity) {
        return Collisions::testCircleEntity(circle, entity);
    });
}

std::vector<Collisions::Contact>
PhysicsManager::boxTest(Registry& registry, const Collisions::Box& box)
{
    return gatherContacts(registry, [&](EntityHandle entity) {
        return Collisions::testBoxEntity(box, entity);
    });
}

Collisions::RayHit PhysicsManager::rayTest(Registry& registry, const Collisions::Ray& ray)
{
    return closestHit(registry, [&](EntityHandle entity) {
        return Collisions::testRayEntity(ray, entity);
    });
}

std::vector<Collisions::RayHit>
PhysicsManager::rayTestAll(Registry& registry, const Collisions::Ray& ray)
{
    return gatherHits(registry, [&](EntityHandle entity) {
        return Collisions::testRayEntity(ray, entity);
    });
}

Collisions::RayHit
PhysicsManager::circleCast(Registry& registry, const Collisions::Ray& path, float radius)
{
    return closestHit(registry, [&](EntityHandle entity) {
        return Collisions::testCircleCastEntity(path, radius, entity);
    });
}

std::vector<Collisions::RayHit>
PhysicsManager::circleCastAll(Registry& registry, const Collisions::Ray& path, float radius)
{
    return gatherHits(registry, [&](EntityHandle entity) {
        return Collisions::testCircleCastEntity(path, radius, entity);
    });
}

Collisions::RayHit
PhysicsManager::boxCast(Registry& registry, const Collisions::Ray& path, const Vec2& halfExtents)
{
    return closestHit(registry, [&](EntityHandle entity) {
        return Collisions::testBoxCastEntity(path, halfExtents, entity);
    });
}

std::vector<Collisions::RayHit>
PhysicsManager::boxCastAll(Registry& registry, const Collisions::Ray& path, const Vec2& halfExtents)
{
    return gatherHits(registry, [&](EntityHandle entity) {
        return Collisions::testBoxCastEntity(path, halfExtents, entity);
    });
}

}   // namespace Engine
