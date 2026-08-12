#pragma once

#include "Collisions.hpp"
#include "PhysicsWorld.hpp"
#include "ecs/registry/Registry.hpp"
#include "utils/Singleton.hpp"
#include <vector>

namespace Engine {

class PhysicsManager : public Singleton<PhysicsManager>
{
    friend class Singleton<PhysicsManager>;

  public:
    void update(Registry& registry, float dt, int steps = 1);
    void substep(Registry& registry, float dt);

    PhysicsWorld& getWorld(Registry& registry);
    const std::vector<ContactRecord>& getContacts(Registry& registry);
    std::vector<ContactRecord> getContacts(Registry& registry, Entity entity);

    std::vector<Entity> pointTest(Registry& registry, const Vec2& point);
    std::vector<Collisions::Contact>
    circleTest(Registry& registry, const Collisions::Circle& circle);
    std::vector<Collisions::Contact> boxTest(Registry& registry, const Collisions::Box& box);
    Collisions::RayHit rayTest(Registry& registry, const Collisions::Ray& ray);
    std::vector<Collisions::RayHit> rayTestAll(Registry& registry, const Collisions::Ray& ray);

    Collisions::RayHit circleCast(Registry& registry, const Collisions::Ray& path, float radius);
    std::vector<Collisions::RayHit>
    circleCastAll(Registry& registry, const Collisions::Ray& path, float radius);
    Collisions::RayHit
    boxCast(Registry& registry, const Collisions::Ray& path, const Vec2& halfExtents);
    std::vector<Collisions::RayHit>
    boxCastAll(Registry& registry, const Collisions::Ray& path, const Vec2& halfExtents);

    Collisions::RayHit sweepTilemaps(Registry& registry, Entity entity, const Vec2& motion);

  private:
    PhysicsManager() = default;
    ~PhysicsManager() = default;

    void beginStep(PhysicsWorld& world);
    void integrate(Registry& registry, PhysicsWorld& world, float dt);
    void depenetrateTilemaps(Registry& registry);
    void collideEntities(Registry& registry, PhysicsWorld& world);
    void dispatchTriggerEvents(PhysicsWorld& world);
};

}   // namespace Engine
