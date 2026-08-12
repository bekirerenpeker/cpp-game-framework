#include "graphics/tilemap/TilemapManager.hpp"
#include "physics/Collisions.hpp"
#include "utils/math/MathFuncs.hpp"

namespace Engine {

namespace Collisions {

namespace {

// Every non-dynamic body answers zero, which is what lets one formula cover both "share the
// push by mass" and "the static one takes none of it" without a branch.
float inverseMassOf(const RigidBodyComponent* body)
{
    if (!body || body->type != BodyType::Dynamic || body->mass <= 0) return 0.0f;
    return 1.0f / body->mass;
}
Vec2 velocityOf(const RigidBodyComponent* body) { return body ? body->velocity : VEC2_ZERO; }

void addVelocity(RigidBodyComponent* body, const Vec2& impulse, float inverseMass)
{
    if (body) body->velocity += impulse * inverseMass;
}

void movePosition(TransformComponent* transform, const Vec2& push)
{
    if (!transform) return;
    transform->position.x += push.x;
    transform->position.y += push.y;
}

}   // namespace

// Tile override first, then the collider's. `tile` is ignored unless the entity is a tilemap.
PhysicsSurfaceOptions surfaceOf(EntityHandle entity, TileCoord tile)
{
    ColliderComponent* collider = entity.tryGet<ColliderComponent>();
    if (!collider) return {};

    if (collider->shape == ColliderShape::Tilemap && !(tile == INVALID_TILE)) {
        TilemapComponent* tilemap = entity.tryGet<TilemapComponent>();
        if (tilemap) {
            const Tileset::TileDefinition* def =
                TilemapManager::get().getDefinitionAt(*tilemap, tile);
            if (def && def->surface) return *def->surface;
        }
    }
    return collider->surface;
}

// Split from resolveContact because a swept mover has already been placed short of the surface
// and only wants the velocity half; correcting its position again would drag it back in.
void correctPositions(
    EntityHandle a, EntityHandle b, const Contact& contact, const ResolveSettings& settings
)
{
    TransformComponent* t1 = a.tryGet<TransformComponent>();
    TransformComponent* t2 = b.tryGet<TransformComponent>();

    // Something with no transform cannot be moved at all.
    float inverseMass1 = t1 ? inverseMassOf(a.tryGet<RigidBodyComponent>()) : 0.0f;
    float inverseMass2 = t2 ? inverseMassOf(b.tryGet<RigidBodyComponent>()) : 0.0f;
    float inverseMassSum = inverseMass1 + inverseMass2;
    if (inverseMassSum <= 0) return;

    // Leaving `penetrationSlop` of overlap uncorrected is what stops a resting body from
    // twitching: correcting to exactly zero every step means it falls back in the next one.
    float correction = Math::max(contact.depth - settings.penetrationSlop, 0.0f) *
                       settings.correctionPercent / inverseMassSum;
    Vec2 push = contact.normal * correction;

    movePosition(t1, push * -inverseMass1);
    movePosition(t2, push * inverseMass2);
}

void applyImpulse(
    EntityHandle a, EntityHandle b, const Contact& contact, const ResolveSettings& settings
)
{
    RigidBodyComponent* body1 = a.tryGet<RigidBodyComponent>();
    RigidBodyComponent* body2 = b.tryGet<RigidBodyComponent>();

    float inverseMass1 = inverseMassOf(body1);
    float inverseMass2 = inverseMassOf(body2);
    float inverseMassSum = inverseMass1 + inverseMass2;
    if (inverseMassSum <= 0) return;

    Vec2 relativeVelocity = velocityOf(body2) - velocityOf(body1);
    float alongNormal = Vec2::dot(relativeVelocity, contact.normal);
    // Already moving apart -- an earlier contact this step has dealt with it, and pulling them
    // together again would make bodies stick.
    if (alongNormal > 0) return;

    PhysicsSurfaceOptions surface1 = surfaceOf(a, contact.tile);
    PhysicsSurfaceOptions surface2 = surfaceOf(b, contact.tile);

    float bounciness = Math::max(surface1.bounciness, surface2.bounciness);
    // Gravity feeds a body a little approach speed every step, so a bouncy one resting on the
    // floor would micro-hop forever without a floor under which a contact counts as a rest.
    if (-alongNormal < settings.bounceThreshold) bounciness = 0.0f;

    float normalImpulse = -(1.0f + bounciness) * alongNormal / inverseMassSum;

    addVelocity(body1, contact.normal * -normalImpulse, inverseMass1);
    addVelocity(body2, contact.normal * normalImpulse, inverseMass2);

    relativeVelocity = velocityOf(body2) - velocityOf(body1);
    Vec2 tangent = relativeVelocity - contact.normal * Vec2::dot(relativeVelocity, contact.normal);
    if (tangent.sqrMagnitude() <= 0) return;
    tangent = tangent.normalized();

    float friction = Math::sqrt(surface1.friction * surface2.friction);
    // Coulomb: friction can slow the slide to a stop but never reverse it, and never exceeds
    // what the normal impulse pressed the surfaces together with.
    float tangentImpulse = -Vec2::dot(relativeVelocity, tangent) / inverseMassSum;
    float maxTangentImpulse = friction * normalImpulse;
    tangentImpulse = Math::clamp(tangentImpulse, -maxTangentImpulse, maxTangentImpulse);

    addVelocity(body1, tangent * -tangentImpulse, inverseMass1);
    addVelocity(body2, tangent * tangentImpulse, inverseMass2);
}

void resolveContact(
    EntityHandle a, EntityHandle b, const Contact& contact, const ResolveSettings& settings
)
{
    correctPositions(a, b, contact, settings);
    applyImpulse(a, b, contact, settings);
}

}   // namespace Collisions

}   // namespace Engine
