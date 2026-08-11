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

void resolveContact(
    EntityHandle a, EntityHandle b, const Contact& contact, const ResolveSettings& settings
)
{
    TransformComponent* t1 = a.tryGet<TransformComponent>();
    TransformComponent* t2 = b.tryGet<TransformComponent>();
    RigidBodyComponent* body1 = a.tryGet<RigidBodyComponent>();
    RigidBodyComponent* body2 = b.tryGet<RigidBodyComponent>();

    // Something with no transform cannot be moved at all, which is how a tilemap absorbs none
    // of the correction without needing a case of its own.
    float inverseMass1 = t1 ? inverseMassOf(body1) : 0.0f;
    float inverseMass2 = t2 ? inverseMassOf(body2) : 0.0f;
    float inverseMassSum = inverseMass1 + inverseMass2;
    if (inverseMassSum <= 0) return;

    // Leaving `penetrationSlop` of overlap uncorrected is what stops a resting body from
    // twitching: correcting to exactly zero every step means it falls back in the next one.
    float correction = Math::max(contact.depth - settings.penetrationSlop, 0.0f) *
                       settings.correctionPercent / inverseMassSum;
    Vec2 push = contact.normal * correction;

    movePosition(t1, push * -inverseMass1);
    movePosition(t2, push * inverseMass2);

    Vec2 relativeVelocity = velocityOf(body2) - velocityOf(body1);
    float alongNormal = Vec2::dot(relativeVelocity, contact.normal);
    // Already moving apart -- an earlier contact this step has dealt with it, and pulling them
    // together again would make bodies stick.
    if (alongNormal > 0) return;

    float bounciness = Math::max(
        body1 ? body1->bounciness : 0.0f,   //
        body2 ? body2->bounciness : 0.0f
    );
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

    float friction =
        Math::sqrt((body1 ? body1->friction : 0.0f) * (body2 ? body2->friction : 0.0f));
    // Coulomb: friction can slow the slide to a stop but never reverse it, and never exceeds
    // what the normal impulse pressed the surfaces together with.
    float tangentImpulse = -Vec2::dot(relativeVelocity, tangent) / inverseMassSum;
    float maxTangentImpulse = friction * normalImpulse;
    tangentImpulse = Math::clamp(tangentImpulse, -maxTangentImpulse, maxTangentImpulse);

    addVelocity(body1, tangent * -tangentImpulse, inverseMass1);
    addVelocity(body2, tangent * tangentImpulse, inverseMass2);
}

}   // namespace Collisions

}   // namespace Engine
