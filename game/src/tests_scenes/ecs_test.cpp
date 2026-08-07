#include "EngineInclude.hpp"
#include "test_funcs.hpp"
#include <chrono>

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

int ecs_test()
{
    auto measure = [](const char* name, auto&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        LOG_INFO("[ECS] {} took {} ms", name, duration / 1000.0f);
    };

    LOG_INFO("============= ECS FEATURE & PERFORMANCE TESTS =============");

    Registry registry;
    constexpr int ENTITY_COUNT = 1'000'000;
    std::vector<EntityHandle> entities;
    entities.reserve(ENTITY_COUNT);

    measure("Create 1,000,000 Entities", [&]() {
        for (int i = 0; i < ENTITY_COUNT; ++i) { entities.push_back(registry.create()); }
    });

    measure("Add Position to all 1M entities", [&]() {
        for (auto& e : entities) { e.emplace<Position>(0.0f, 0.0f); }
    });

    measure("Add Velocity to 500K entities", [&]() {
        for (int i = 0; i < ENTITY_COUNT; i += 2) { entities[i].emplace<Velocity>(1.0f, 1.0f); }
    });

    measure("Add PlayerTag (Empty) to 250K entities", [&]() {
        for (int i = 0; i < ENTITY_COUNT; i += 4) { entities[i].emplace<PlayerTag>(); }
    });

    measure("Add Static (Empty) to 250K entities", [&]() {
        for (int i = 1; i < ENTITY_COUNT; i += 4) { entities[i].emplace<Static>(); }
    });

    LOG_INFO("-----------------------------------------------------------");

    size_t countSingle = 0;
    measure("Iterate Single Type (Position, 1M entities)", [&]() {
        View<Position> view(registry);
        view.each([&](Entity e, Position& p) {
            p.x += 1.0f;
            p.y += 1.0f;
            countSingle++;
        });
    });
    if (countSingle != 1'000'000) LOG_ERROR("Expected 1,000,000 but got {}", countSingle);

    size_t countMulti = 0;
    measure("Iterate Multi Type (Position, Velocity, 500K matches)", [&]() {
        View<Position, Velocity> view(registry);
        view.each([&](Entity e, Position& p, Velocity& v) {
            p.x += v.dx;
            p.y += v.dy;
            countMulti++;
        });
    });
    if (countMulti != 500'000) LOG_ERROR("Expected 500,000 but got {}", countMulti);

    size_t countTag = 0;
    measure("Iterate with Tag (Position, Velocity, PlayerTag, 250K matches)", [&]() {
        View<Position, Velocity, PlayerTag> view(registry);
        view.each([&](Entity e, Position& p, Velocity& v, PlayerTag&) {
            p.x *= 2.0f;
            countTag++;
        });
    });
    if (countTag != 250'000) LOG_ERROR("Expected 250,000 but got {}", countTag);

    // Static is on indices 1, 5, 9...; Velocity is on 0, 2, 4... -- they never overlap,
    // so all 500K survive the exclusion.
    size_t countExcl = 0;
    measure("Iterate with Exclude (Pos, Vel, Exclude<Static>, 500K matches)", [&]() {
        View<Position, Velocity, Exclude<Static>> view(registry);
        view.each([&](Entity e, Position& p, Velocity& v) {
            p.y -= v.dy;
            countExcl++;
        });
    });
    if (countExcl != 500'000) LOG_ERROR("Expected 500,000 but got {}", countExcl);

    LOG_INFO("-----------------------------------------------------------");

    measure("Test Component Signals (onCreate, onSet, onDestroy)", [&]() {
        SignalTracker tracker;
        auto& posPool = registry.getPool<Position>();

        posPool.onCreate().connect<&SignalTracker::onCreated, SignalTracker>(&tracker);
        posPool.onSet().connect<&SignalTracker::onSet, SignalTracker>(&tracker);
        posPool.onDestroy().connect<&SignalTracker::onDestroyed, SignalTracker>(&tracker);

        EntityHandle sigEntity = registry.create();

        sigEntity.emplace<Position>(1.0f, 1.0f);
        sigEntity.emplace<Position>(2.0f, 2.0f);
        sigEntity.destroy();

        if (tracker.created != 1 || tracker.set != 1 || tracker.destroyed != 1) {
            LOG_ERROR(
                "Signals failed! Created: {} Set: {} Destroyed: {}", tracker.created, tracker.set,
                tracker.destroyed
            );
        }

        // CRITICAL: disconnect before tracker goes out of scope, or the mass
        // destruction below crashes calling into the destroyed object.
        posPool.onCreate().disconnect<&SignalTracker::onCreated, SignalTracker>(&tracker);
        posPool.onSet().disconnect<&SignalTracker::onSet, SignalTracker>(&tracker);
        posPool.onDestroy().disconnect<&SignalTracker::onDestroyed, SignalTracker>(&tracker);
    });

    measure("Test Clone Entity", [&]() {
        EntityHandle source = registry.create();
        source.emplace<Position>(10.0f, 20.0f);
        source.emplace<Health>(100);
        source.emplace<PlayerTag>();

        EntityHandle cloned = source.clone();
        if (!cloned.containsAll<Position, Health, PlayerTag>()) LOG_ERROR("Clone feature failed!");
    });

    measure("Test tryGet()", [&]() {
        EntityHandle source = registry.create();
        source.emplace<Health>(50);

        Health* hp = source.tryGet<Health>();
        Velocity* vel = source.tryGet<Velocity>();

        if (hp == nullptr || hp->hp != 50 || vel != nullptr) LOG_ERROR("tryGet feature failed!");
    });

    measure("Test Deferred Destruction", [&]() {
        size_t initialSize = registry.getPool<Position>().size();

        registry.destroyDeferred(entities[0].getEntity());
        registry.destroyDeferred(entities[1].getEntity());

        if (registry.getPool<Position>().size() != initialSize)
            LOG_ERROR("Deferred destruction removed an entity before flush!");

        registry.flush();

        if (registry.getPool<Position>().size() != initialSize - 2)
            LOG_ERROR("Deferred destruction flush failed!");
    });

    LOG_INFO("-----------------------------------------------------------");

    measure("Destroy remaining ~1,000,000 Entities", [&]() {
        for (int i = 2; i < ENTITY_COUNT; ++i) entities[i].destroy();
    });

    LOG_INFO("===========================================================\n");

    return 0;
}
