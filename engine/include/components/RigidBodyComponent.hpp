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
//
// bounciness is a property of the pair, not of one body: a contact takes the larger of the
// two, so one bouncy object bounces off anything. friction takes the geometric mean instead,
// so one slippery surface is enough to slide on. Both are 0..1.
struct RigidBodyComponent
{
    Vec2 velocity = VEC2_ZERO;
    float mass = 1.0f, bounciness = 0.0f, friction = 0.0f, gravityScale = 1.0f;
    BodyType type = BodyType::Dynamic;
};

}   // namespace Engine
