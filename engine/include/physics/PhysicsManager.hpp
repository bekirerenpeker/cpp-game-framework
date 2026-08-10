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
    void step(Registry& registry, float dt);

    PhysicsWorld& getWorld(Registry& registry);
    const std::vector<ContactRecord>& getContacts(Registry& registry);
    std::vector<ContactRecord> getContacts(Registry& registry, Entity entity);

    std::vector<Entity> pointTest(Registry& registry, const Vec2& point);
    std::vector<Entity> circleTest(Registry& registry, const Collisions::Circle& circle);
    std::vector<Entity> boxTest(Registry& registry, const Collisions::Box& box);
    Collisions::RayHit rayTest(Registry& registry, const Collisions::Ray& ray);
    std::vector<Collisions::RayHit> rayTestAll(Registry& registry, const Collisions::Ray& ray);

  private:
    PhysicsManager() = default;
    ~PhysicsManager() = default;

    void beginStep(PhysicsWorld& world);
    void integrate(Registry& registry, PhysicsWorld& world, float dt);
    void collideEntities(Registry& registry, PhysicsWorld& world);
    void dispatchTriggerEvents(PhysicsWorld& world);
};

}   // namespace Engine
