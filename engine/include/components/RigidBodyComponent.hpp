#pragma once

#include "utils/TypeAliases.hpp"
#include "utils/math/Vec2.hpp"

namespace Engine {

enum class BodyType : uint8_t
{
    Static,
    Kinematic,
    Dynamic
};

// How an entity responds to a contact, orthogonal to what the contact means (see
// ColliderComponent::isTrigger).
//   Static     -- never integrated, never displaced. Blocks everything, moves for nothing;
//                 the same as owning no rigidbody at all, but switchable at runtime.
//   Kinematic  -- integrated by its own velocity and pushes dynamic bodies out of the way,
//                 but resolution never moves it. Moving platforms, elevators, doors.
//   Dynamic    -- integrated and displaced by resolution, sharing the correction with the
//                 other body by mass.
// mass is what the caller authors; the solver derives the inverse and treats every
// non-dynamic body as infinite, so a dynamic body hitting one takes the whole correction.
// Bounciness and friction describe the surface, so they live on ColliderComponent.
struct RigidBodyComponent
{
    Vec2 velocity = VEC2_ZERO;
    Vec2 forceAccumulator = VEC2_ZERO;
    float mass = 1.0f, gravityScale = 1.0f, damping = 0.0f;
    BodyType type = BodyType::Dynamic;

    void addForce(const Vec2& force)
    {
        if (type == BodyType::Dynamic) forceAccumulator += force;
    }
    void addAcceleration(const Vec2& acceleration)
    {
        if (type == BodyType::Dynamic) forceAccumulator += acceleration * mass;
    }

    void addImpulse(const Vec2& impulse)
    {
        if (type == BodyType::Dynamic && mass > 0) velocity += impulse / mass;
    }
    void addVelocityChange(const Vec2& velocityChange)
    {
        if (type == BodyType::Dynamic) velocity += velocityChange;
    }
};

}   // namespace Engine
