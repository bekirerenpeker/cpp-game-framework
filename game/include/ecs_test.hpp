#pragma once

#include "EngineInclude.hpp"
#include <chrono>
#include <iostream>

using namespace Engine;

// --- Dummy Components for Testing ---
struct Position
{
    float x, y;
};
struct Velocity
{
    float dx, dy;
};
struct Health
{
    int hp;
};
struct PlayerTag
{
};   // Empty component to test empty optimizations
struct EnemyTag
{
};   // Empty component
struct Static
{
};   // Empty component to test exclusions

// --- Dummy Tracker for Signals ---
struct SignalTracker
{
    int created = 0;
    int set = 0;
    int destroyed = 0;

    void onCreated(Entity e) { created++; }
    void onSet(Entity e) { set++; }
    void onDestroyed(Entity e) { destroyed++; }
};

inline int ecs_test()
{
    // Helper lambda to measure block execution time accurately using std::chrono
    auto measure = [](const char* name, auto&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        std::cout << "[ECS] " << name << " took " << (duration / 1000.0f) << " ms\n";
    };

    std::cout << "============= ECS FEATURE & PERFORMANCE TESTS =============\n";

    Registry registry;
    constexpr int ENTITY_COUNT = 1'000'000;
    std::vector<EntityHandle> entities;
    entities.reserve(ENTITY_COUNT);

    // 1. Entity Creation
    measure("Create 1,000,000 Entities", [&]() {
        for (int i = 0; i < ENTITY_COUNT; ++i) { entities.push_back(registry.create()); }
    });

    // 2. Add Components (Dense Population)
    measure("Add Position to all 1M entities", [&]() {
        for (auto& e : entities) { e.emplace<Position>(0.0f, 0.0f); }
    });

    // 3. Add Components (Sparse Population)
    measure("Add Velocity to 500K entities", [&]() {
        for (int i = 0; i < ENTITY_COUNT; i += 2) { entities[i].emplace<Velocity>(1.0f, 1.0f); }
    });

    // 4. Add Tags (Testing if constexpr isEmpty optimization)
    measure("Add PlayerTag (Empty) to 250K entities", [&]() {
        for (int i = 0; i < ENTITY_COUNT; i += 4) { entities[i].emplace<PlayerTag>(); }
    });

    measure("Add Static (Empty) to 250K entities", [&]() {
        for (int i = 1; i < ENTITY_COUNT; i += 4) { entities[i].emplace<Static>(); }
    });

    std::cout << "-----------------------------------------------------------\n";

    // 5. Iterate Single Type
    size_t countSingle = 0;
    measure("Iterate Single Type (Position, 1M entities)", [&]() {
        View<Position> view(registry);
        view.each([&](Entity e, Position& p) {
            p.x += 1.0f;
            p.y += 1.0f;
            countSingle++;
        });
    });
    if (countSingle == 1'000'000)
        std::cout << "      SUCCESS: Counted exactly 1,000,000 entities.\n";
    else std::cout << "      ERROR: Expected 1,000,000 but got " << countSingle << "\n";

    // 6. Iterate Multi Type (Testing driver pool optimization)
    size_t countMulti = 0;
    measure("Iterate Multi Type (Position, Velocity, 500K matches)", [&]() {
        View<Position, Velocity> view(registry);
        view.each([&](Entity e, Position& p, Velocity& v) {
            p.x += v.dx;
            p.y += v.dy;
            countMulti++;
        });
    });
    if (countMulti == 500'000) std::cout << "      SUCCESS: Counted exactly 500,000 entities.\n";
    else std::cout << "      ERROR: Expected 500,000 but got " << countMulti << "\n";

    // 7. Iterate Multi Type with Tags
    size_t countTag = 0;
    measure("Iterate with Tag (Position, Velocity, PlayerTag, 250K matches)", [&]() {
        View<Position, Velocity, PlayerTag> view(registry);
        view.each([&](Entity e, Position& p, Velocity& v, PlayerTag&) {
            p.x *= 2.0f;
            countTag++;
        });
    });
    if (countTag == 250'000) std::cout << "      SUCCESS: Counted exactly 250,000 entities.\n";
    else std::cout << "      ERROR: Expected 250,000 but got " << countTag << "\n";

    // 8. Iterate Multi Type with Exclusions!
    size_t countExcl = 0;
    measure("Iterate with Exclude (Pos, Vel, Exclude<Static>, 500K matches)", [&]() {
        View<Position, Velocity, Exclude<Static>> view(registry);
        view.each([&](Entity e, Position& p, Velocity& v) {
            p.y -= v.dy;
            countExcl++;
        });
    });
    // Static is on indices 1, 5, 9... Velocity is on 0, 2, 4... They never overlap! So all 500K should remain.
    if (countExcl == 500'000) std::cout << "      SUCCESS: Counted exactly 500,000 entities.\n";
    else std::cout << "      ERROR: Expected 500,000 but got " << countExcl << "\n";

    std::cout << "-----------------------------------------------------------\n";

    // 9. Test Component Signals
    measure("Test Component Signals (onCreate, onSet, onDestroy)", [&]() {
        SignalTracker tracker;
        auto& posPool = registry.getPool<Position>();

        // Connect our tracker methods to the Sinks
        posPool.onCreate().connect<&SignalTracker::onCreated, SignalTracker>(&tracker);
        posPool.onSet().connect<&SignalTracker::onSet, SignalTracker>(&tracker);
        posPool.onDestroy().connect<&SignalTracker::onDestroyed, SignalTracker>(&tracker);

        EntityHandle sigEntity = registry.create();

        sigEntity.emplace<Position>(1.0f, 1.0f);   // Should trigger onCreate
        sigEntity.emplace<Position>(2.0f, 2.0f);   // Should trigger onSet
        sigEntity.destroy();                       // Should trigger onDestroy

        if (tracker.created == 1 && tracker.set == 1 && tracker.destroyed == 1) {
            std::cout
                << "      SUCCESS: Signals triggered perfectly (1 Create, 1 Set, 1 Destroy).\n";
        } else {
            std::cout << "      ERROR: Signals failed! Created: " << tracker.created
                      << " Set: " << tracker.set << " Destroyed: " << tracker.destroyed << "\n";
        }

        // CRITICAL: Disconnect listeners before tracker goes out of scope,
        // otherwise mass destruction below will crash trying to call the destroyed object!
        posPool.onCreate().disconnect<&SignalTracker::onCreated, SignalTracker>(&tracker);
        posPool.onSet().disconnect<&SignalTracker::onSet, SignalTracker>(&tracker);
        posPool.onDestroy().disconnect<&SignalTracker::onDestroyed, SignalTracker>(&tracker);
    });

    // 10. Test Clone Feature
    measure("Test Clone Entity", [&]() {
        EntityHandle source = registry.create();
        source.emplace<Position>(10.0f, 20.0f);
        source.emplace<Health>(100);
        source.emplace<PlayerTag>();

        EntityHandle cloned = source.clone();
        if (cloned.containsAll<Position, Health, PlayerTag>()) {
            std::cout << "      SUCCESS: Cloned entity successfully inherited all components.\n";
        } else {
            std::cout << "      ERROR: Clone feature failed!\n";
        }
    });

    // 11. Test TryGet Feature
    measure("Test tryGet()", [&]() {
        EntityHandle source = registry.create();
        source.emplace<Health>(50);

        Health* hp = source.tryGet<Health>();
        Velocity* vel = source.tryGet<Velocity>();

        if (hp != nullptr && hp->hp == 50 && vel == nullptr) {
            std::cout << "      SUCCESS: tryGet returns pointers correctly.\n";
        } else {
            std::cout << "      ERROR: tryGet feature failed!\n";
        }
    });

    // 12. Test Deferred Destruction
    measure("Test Deferred Destruction", [&]() {
        size_t initialSize = registry.getPool<Position>().size();

        registry.destroyDeferred(entities[0].getEntity());
        registry.destroyDeferred(entities[1].getEntity());

        if (registry.getPool<Position>().size() == initialSize) {
            std::cout << "      SUCCESS: Entities safely deferred. (Size unchanged)\n";
        }

        registry.flush();

        if (registry.getPool<Position>().size() == initialSize - 2) {
            std::cout << "      SUCCESS: Flush executed destructions. (Size -2)\n";
        } else {
            std::cout << "      ERROR: Deferred destruction flush failed!\n";
        }
    });

    std::cout << "-----------------------------------------------------------\n";

    // 13. Mass Destruction
    measure("Destroy remaining ~1,000,000 Entities", [&]() {
        for (int i = 2; i < ENTITY_COUNT; ++i) {   // Start from 2 because 0,1 were destroyed
            entities[i].destroy();
        }
    });

    std::cout << "===========================================================\n\n";

    return 0;
}
