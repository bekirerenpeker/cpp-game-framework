#pragma once

#include "ecs/registry/Entity.hpp"
#include "ecs/signals/Signal.hpp"
#include "physics/Collisions.hpp"
#include "utils/math/Vec2.hpp"
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace Engine {

// impactSpeed is the relative velocity along the normal sampled before resolution ran, since
// the solver zeroes that component and gameplay cannot recover it afterwards.
struct ContactRecord
{
    Entity a = NULL_ENTITY, b = NULL_ENTITY;
    Collisions::Contact contact;
    float impactSpeed = 0.0f;
    bool isTrigger = false;
};

struct PhysicsWorld
{
    Vec2 gravity = Vec2(0.0f, -9.81f);

    Collisions::ResolveSettings resolveSettings;

    std::vector<ContactRecord> contacts;

    Signal<void(const ContactRecord&)> onTriggerEnter;
    Signal<void(const ContactRecord&)> onTriggerExit;

    std::unordered_set<uint64_t> triggerPairs;
    std::unordered_set<uint64_t> prevTriggerPairs;

    static uint64_t pairKey(Entity a, Entity b)
    {
        Entity low = a < b ? a : b;
        Entity high = a < b ? b : a;
        return (static_cast<uint64_t>(low) << 32) | static_cast<uint64_t>(high);
    }
    static Entity pairFirst(uint64_t key) { return static_cast<Entity>(key >> 32); }
    static Entity pairSecond(uint64_t key) { return static_cast<Entity>(key & 0xFFFFFFFF); }
};

}   // namespace Engine
