#pragma once

#include "utils/TypeAliases.hpp"
#include "utils/math/Vec2.hpp"

namespace Engine {

enum class ColliderShape : uint8_t
{
    Box,
    Circle,
    Tilemap
};

// Both are pair properties: a contact takes the larger bounciness and the geometric mean of
// the frictions. On the collider rather than the body so a wall and a single tile have them
// too. Both 0..1.
struct PhysicsSurfaceOptions
{
    float bounciness = 0.0f;
    float friction = 0.5f;
};

// Owning this is what makes an entity collidable; owning no RigidBodyComponent alongside it
// makes that collider static. A Tilemap shape carries no geometry of its own and reads the
// sibling TilemapComponent instead.
//
// isTrigger says what a contact *means*, where BodyType says how the entity *responds*, so
// the two combine freely:
//   solid  + no rigidbody -- a wall. Blocks, never moves.
//   solid  + Dynamic      -- an ordinary physical body.
//   trigger + no rigidbody -- a region. Reports what overlaps it, blocks nothing.
//   trigger + Dynamic      -- a moving probe. Falls and travels, passes through everything.
// A trigger is always detected and never resolved: neither side is pushed, whatever their
// body types.
struct ColliderComponent
{
    ColliderShape shape = ColliderShape::Box;

    Vec2 halfExtents = VEC2_ONE * 0.5f;   // Box
    float radius = 0.5f;                  // Circle
    Vec2 offset = VEC2_ZERO;

    // A Tilemap uses this only where a tile does not override it.
    PhysicsSurfaceOptions surface;

    LayerMask collidesWith = LAYER_ALL;

    bool isTrigger = false;
};

}   // namespace Engine
