#pragma once

#include "Collisions.hpp"
#include "PhysicsWorld.hpp"
#include "components/RigidBodyComponent.hpp"
#include "components/TransformComponent.hpp"
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
    void beginStep(PhysicsWorld& world);
    void integrate(Registry& registry, PhysicsWorld& world, float dt);
    void collideEntities(Registry& registry, PhysicsWorld& world);
    void dispatchTriggerEvents(PhysicsWorld& world);

    void resolve(
        TransformComponent& t1, RigidBodyComponent* body1, TransformComponent& t2,
        RigidBodyComponent* body2, const Collisions::Contact& contact
    );

    PhysicsManager() = default;
    ~PhysicsManager() = default;
};

}   // namespace Engine
