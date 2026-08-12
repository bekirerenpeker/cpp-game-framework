#include "physics/PhysicsManager.hpp"
#include "ecs/registry/View.hpp"
#include "components/RigidBodyComponent.hpp"
#include "components/TransformComponent.hpp"
#include "physics/Collisions.hpp"

namespace Engine {

namespace {

// A sweep that lands exactly on a surface starts the next one already touching, where a cast
// has no entry face to report a normal from.
constexpr float SWEEP_SKIN = 0.01f;
constexpr int DEPENETRATION_PASSES = 4;

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
    // Last, because the pair loop is what pushes bodies into tiles: it splits a separation by
    // inverse mass with no idea one side is resting on a tilemap. Ending the step outside the
    // tiles is what lets the next sweep start from a surface it can read a normal off.
    depenetrateTilemaps(registry);

    dispatchTriggerEvents(world);
}

Collisions::RayHit
PhysicsManager::sweepTilemaps(Registry& registry, Entity entity, const Vec2& motion)
{
    Collisions::RayHit nearest;
    if (motion == VEC2_ZERO) return nearest;

    EntityHandle mover(entity, registry);

    View<TilemapComponent> tilemaps(registry);
    tilemaps.each([&](Entity tilemapEntity, TilemapComponent&) {
        Collisions::RayHit hit =
            Collisions::testEntityCastEntity(motion, mover, EntityHandle(tilemapEntity, registry));
        if (!hit.isHit) return;
        if (nearest.isHit && hit.distance >= nearest.distance) return;

        hit.entity = tilemapEntity;
        nearest = hit;
    });
    return nearest;
}

void PhysicsManager::depenetrateTilemaps(Registry& registry)
{
    std::vector<Entity> tilemapEntities;
    View<TilemapComponent> tilemapView(registry);
    tilemapView.each([&](Entity entity, TilemapComponent&) { tilemapEntities.push_back(entity); });
    if (tilemapEntities.empty()) return;

    View<TransformComponent, RigidBodyComponent, ColliderComponent> view(registry);
    view.each([&](Entity entity, TransformComponent& transform, RigidBodyComponent& body,
                  ColliderComponent&) {
        if (body.type != BodyType::Dynamic) return;

        EntityHandle mover(entity, registry);

        // One Contact only ever reports the deepest tile, so a body wedged into a corner takes
        // more than one push to get clear.
        for (int pass = 0; pass < DEPENETRATION_PASSES; pass++) {
            bool pushed = false;

            for (Entity tilemapEntity : tilemapEntities) {
                Collisions::Contact contact =
                    Collisions::testEntities(mover, EntityHandle(tilemapEntity, registry));
                if (!contact.isTouching) continue;

                // The normal runs body -> tile, and the whole correction lands on the body:
                // nothing moves a tilemap.
                Vec2 escape = contact.normal * -(contact.depth + SWEEP_SKIN);
                transform.position.x += escape.x;
                transform.position.y += escape.y;

                // Whatever velocity drove it in would drive it straight back, and the sweep
                // never got to run its impulse because the cast had no normal to report.
                float into = Vec2::dot(body.velocity, contact.normal);
                if (into > 0) body.velocity -= contact.normal * into;

                pushed = true;
            }
            if (!pushed) break;
        }
    });
}

void PhysicsManager::integrate(Registry& registry, PhysicsWorld& world, float dt)
{
    // One axis at a time, and swept rather than moved outright: a tile grid's interior faces
    // are not real surfaces, and pushing out of a two-axis overlap keeps picking them, which
    // slides a body sideways along what should be a flat floor. With no tilemap in the registry
    // every sweep misses and this is just position += velocity * dt.
    auto sweepAxis = [&](Entity entity, float& position, float want, bool horizontal) {
        Collisions::RayHit hit =
            sweepTilemaps(registry, entity, horizontal ? Vec2(want, 0.0f) : Vec2(0.0f, want));

        // A zero normal means the sweep began already touching, which leaves no direction to
        // resolve along; letting the move through is what keeps an embedded body able to leave.
        if (!hit.isHit || hit.normal == VEC2_ZERO) {
            position += want;
            return;
        }

        // Stopping a hair short leaves the next sweep starting outside the surface, where it
        // still has a normal to report.
        float travelled = Math::max(hit.distance - SWEEP_SKIN, 0.0f);
        position += want > 0 ? travelled : -travelled;

        Collisions::Contact contact;
        contact.isTouching = true;
        contact.point = hit.point;
        contact.normal = -hit.normal;
        contact.depth = 0.0f;

        // The position is already handled by stopping short, so only the velocity half runs --
        // the same bounciness, rest threshold and friction every other collider goes through.
        Collisions::applyImpulse(
            EntityHandle(entity, registry), EntityHandle(hit.entity, registry), contact,
            world.resolveSettings
        );
    };

    View<TransformComponent, RigidBodyComponent> view(registry);
    view.each([&](Entity entity, TransformComponent& transform, RigidBodyComponent& body) {
        if (body.type == BodyType::Static) return;

        // Kinematic bodies carry whatever velocity the caller gave them and nothing else acts
        // on them, which is what makes a moving platform hold its path.
        if (body.type == BodyType::Dynamic) {
            body.velocity += world.gravity * body.gravityScale * dt;
        }

        sweepAxis(entity, transform.position.x, body.velocity.x * dt, true);
        sweepAxis(entity, transform.position.y, body.velocity.y * dt, false);
    });
}

void PhysicsManager::collideEntities(Registry& registry, PhysicsWorld& world)
{
    std::vector<Entity> entities;
    View<ColliderComponent> colliderView(registry);
    colliderView.each([&](Entity entity, ColliderComponent& collider) {
        // The tilemap tests exist, but a body wedged between tiles needs axis-separated
        // movement rather than one MTV push, so tilemaps stay out of the pair loop until
        // that lands.
        if (collider.shape == ColliderShape::Tilemap) return;
        entities.push_back(entity);
    });

    SparseSet<ColliderComponent>& colliders = registry.getPool<ColliderComponent>();
    SparseSet<RigidBodyComponent>& bodies = registry.getPool<RigidBodyComponent>();

    for (size_t i = 0; i < entities.size(); i++) {
        for (size_t j = i + 1; j < entities.size(); j++) {
            Entity a = entities[i], b = entities[j];

            RigidBodyComponent* bodyA = bodies.contains(a) ? &bodies.get(a) : nullptr;
            RigidBodyComponent* bodyB = bodies.contains(b) ? &bodies.get(b) : nullptr;
            if (isStatic(bodyA) && isStatic(bodyB)) continue;

            EntityHandle handleA(a, registry), handleB(b, registry);

            Collisions::Contact contact = Collisions::testEntities(handleA, handleB);
            if (!contact.isTouching) continue;

            ContactRecord record;
            record.a = a;
            record.b = b;
            record.contact = contact;
            record.isTrigger = colliders.get(a).isTrigger || colliders.get(b).isTrigger;

            // Sampled before resolve zeroes the velocity along the normal, and negated so an
            // approach reads positive; the normal runs a -> b.
            Vec2 relativeVelocity = velocityOf(bodyB) - velocityOf(bodyA);
            record.impactSpeed = -Vec2::dot(relativeVelocity, contact.normal);

            world.contacts.push_back(record);

            if (record.isTrigger) {
                world.triggerPairs.insert(PhysicsWorld::pairKey(a, b));
                continue;
            }
            Collisions::resolveContact(handleA, handleB, contact, world.resolveSettings);
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
