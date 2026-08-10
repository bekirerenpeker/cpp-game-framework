#include "physics/PhysicsManager.hpp"
#include "ecs/registry/View.hpp"
#include "components/RigidBodyComponent.hpp"
#include "components/TransformComponent.hpp"
#include "physics/Collisions.hpp"

namespace Engine {

namespace {

bool isStatic(const RigidBodyComponent* body)
{
    return body == nullptr || body->type == BodyType::Static;
}
Vec2 velocityOf(const RigidBodyComponent* body) { return body ? body->velocity : VEC2_ZERO; }

}   // namespace

void PhysicsManager::step(Registry& registry, float dt)
{
    PhysicsWorld& world = getWorld(registry);
    beginStep(world);

    integrate(registry, world, dt);
    collideEntities(registry, world);

    dispatchTriggerEvents(world);
}

void PhysicsManager::integrate(Registry& registry, PhysicsWorld& world, float dt)
{
    View<TransformComponent, RigidBodyComponent> view(registry);
    view.each([&](Entity, TransformComponent& transform, RigidBodyComponent& body) {
        if (body.type == BodyType::Static) return;

        // Kinematic bodies carry whatever velocity the caller gave them and nothing else acts
        // on them, which is what makes a moving platform hold its path.
        if (body.type == BodyType::Dynamic) {
            body.velocity += world.gravity * body.gravityScale * dt;
        }

        transform.position.x += body.velocity.x * dt;
        transform.position.y += body.velocity.y * dt;
    });
}

void PhysicsManager::collideEntities(Registry& registry, PhysicsWorld& world)
{
    std::vector<Entity> entities;
    View<TransformComponent, ColliderComponent> colliderView(registry);
    colliderView.each([&](Entity entity, TransformComponent&, ColliderComponent& collider) {
        // Tilemaps hold no geometry testColliders can reach; they get their own phase.
        if (collider.shape == ColliderShape::Tilemap) return;
        entities.push_back(entity);
    });

    SparseSet<TransformComponent>& transforms = registry.getPool<TransformComponent>();
    SparseSet<ColliderComponent>& colliders = registry.getPool<ColliderComponent>();
    SparseSet<RigidBodyComponent>& bodies = registry.getPool<RigidBodyComponent>();

    for (size_t i = 0; i < entities.size(); i++) {
        for (size_t j = i + 1; j < entities.size(); j++) {
            Entity a = entities[i], b = entities[j];

            RigidBodyComponent* bodyA = bodies.contains(a) ? &bodies.get(a) : nullptr;
            RigidBodyComponent* bodyB = bodies.contains(b) ? &bodies.get(b) : nullptr;
            if (isStatic(bodyA) && isStatic(bodyB)) continue;

            TransformComponent& transformA = transforms.get(a);
            TransformComponent& transformB = transforms.get(b);
            ColliderComponent& colliderA = colliders.get(a);
            ColliderComponent& colliderB = colliders.get(b);

            Collisions::Contact contact =
                Collisions::testColliders(transformA, colliderA, transformB, colliderB);
            if (!contact.isTouching) continue;

            ContactRecord record;
            record.a = a;
            record.b = b;
            record.contact = contact;
            record.isTrigger = colliderA.isTrigger || colliderB.isTrigger;

            // Sampled before resolve zeroes the velocity along the normal, and negated so an
            // approach reads positive; the normal runs a -> b.
            Vec2 relativeVelocity = velocityOf(bodyB) - velocityOf(bodyA);
            record.impactSpeed = -Vec2::dot(relativeVelocity, contact.normal);

            world.contacts.push_back(record);

            if (record.isTrigger) {
                world.triggerPairs.insert(PhysicsWorld::pairKey(a, b));
                continue;
            }
            Collisions::resolveContact(
                transformA, bodyA, transformB, bodyB, contact, world.resolveSettings
            );
        }
    }
}

PhysicsWorld& PhysicsManager::getWorld(Registry& registry)
{
    return registry.getContext<PhysicsWorld>();
}

const std::vector<ContactRecord>& PhysicsManager::getContacts(Registry& registry)
{
    return getWorld(registry).contacts;
}

// Records are stored once per pair with the normal running a -> b; a caller asking about one
// entity gets it back as `a` with the normal pointing away from itself, so nothing downstream
// has to work out which side it was on.
std::vector<ContactRecord> PhysicsManager::getContacts(Registry& registry, Entity entity)
{
    std::vector<ContactRecord> out;
    for (const ContactRecord& record : getWorld(registry).contacts) {
        if (record.a == entity) {
            out.push_back(record);
        } else if (record.b == entity) {
            ContactRecord flipped = record;
            flipped.a = record.b;
            flipped.b = record.a;
            flipped.contact.normal = -record.contact.normal;
            out.push_back(flipped);
        }
    }
    return out;
}

void PhysicsManager::beginStep(PhysicsWorld& world)
{
    world.contacts.clear();
    world.prevTriggerPairs = std::move(world.triggerPairs);
    world.triggerPairs.clear();
}

void PhysicsManager::dispatchTriggerEvents(PhysicsWorld& world)
{
    for (const ContactRecord& record : world.contacts) {
        if (!record.isTrigger) continue;
        if (world.prevTriggerPairs.count(PhysicsWorld::pairKey(record.a, record.b))) continue;
        world.onTriggerEnter.publish(record);
    }

    for (uint64_t key : world.prevTriggerPairs) {
        if (world.triggerPairs.count(key)) continue;

        // An exit has no geometry left to report -- the pair is no longer touching -- so the
        // record carries the entities and nothing else.
        ContactRecord record;
        record.a = PhysicsWorld::pairFirst(key);
        record.b = PhysicsWorld::pairSecond(key);
        record.isTrigger = true;
        world.onTriggerExit.publish(record);
    }
}

}   // namespace Engine
