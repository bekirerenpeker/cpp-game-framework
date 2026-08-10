#include "EngineInclude.hpp"
#include "physics/PhysicsManager.hpp"
#include "test_funcs.hpp"
#include <format>
#include <string>
#include <unordered_set>
#include <vector>

using namespace Engine;

namespace {

struct SpawnSettings
{
    float speedRange = 280.0f;
    float liftMin = -200.0f;
    float liftMax = 320.0f;
    float bouncinessMin = 0.0f;
    float bouncinessMax = 0.9f;
    float friction = 0.3f;
};

// The trigger box publishes onTriggerEnter/onTriggerExit once per pair per crossing; counting
// them here is what shows the diffing works rather than firing every frame the pair overlaps.
struct TriggerLog
{
    Registry* registry = nullptr;
    int enterCount = 0;
    int exitCount = 0;
    std::unordered_set<Entity> inside;
    std::vector<std::string> lines;

    void onEnter(const ContactRecord& record);
    void onExit(const ContactRecord& record);
    void clear();
    void push(std::string line);
};

SpawnSettings g_spawn;
TriggerLog g_triggerLog;

Entity triggerPartner(Registry& registry, const ContactRecord& record)
{
    SparseSet<ColliderComponent>& colliders = registry.getPool<ColliderComponent>();
    if (colliders.contains(record.a) && colliders.get(record.a).isTrigger) return record.b;
    return record.a;
}

void TriggerLog::onEnter(const ContactRecord& record)
{
    Entity entity = triggerPartner(*registry, record);
    enterCount++;
    inside.insert(entity);
    push(std::format("enter  entity {}", entity));
}

void TriggerLog::onExit(const ContactRecord& record)
{
    Entity entity = triggerPartner(*registry, record);
    exitCount++;
    inside.erase(entity);
    push(std::format("exit   entity {}", entity));
}

void TriggerLog::clear()
{
    enterCount = 0;
    exitCount = 0;
    inside.clear();
    lines.clear();
}

void TriggerLog::push(std::string line)
{
    lines.insert(lines.begin(), std::move(line));
    if (lines.size() > 6) lines.pop_back();
}

EntityHandle makeCollider(Registry& registry, Vec2 pos, BodyType type, bool hasBody)
{
    EntityHandle entity = registry.create();
    entity.emplace<TransformComponent>().position = Vec3(pos.x, pos.y, 0);
    if (hasBody) entity.emplace<RigidBodyComponent>().type = type;
    return entity;
}

EntityHandle makeBox(
    Registry& registry, Vec2 pos, Vec2 halfExtents, BodyType type, bool hasBody = true,
    bool isTrigger = false
)
{
    EntityHandle entity = makeCollider(registry, pos, type, hasBody);
    ColliderComponent& collider = entity.emplace<ColliderComponent>();
    collider.shape = ColliderShape::Box;
    collider.halfExtents = halfExtents;
    collider.isTrigger = isTrigger;
    return entity;
}

EntityHandle makeCircle(Registry& registry, Vec2 pos, float radius, BodyType type)
{
    EntityHandle entity = makeCollider(registry, pos, type, true);
    ColliderComponent& collider = entity.emplace<ColliderComponent>();
    collider.shape = ColliderShape::Circle;
    collider.radius = radius;
    return entity;
}

Color colliderColor(const RigidBodyComponent* body, const ColliderComponent& collider)
{
    if (collider.isTrigger) return Color(1.0f, 0.85f, 0.2f);
    if (!body || body->type == BodyType::Static) return Color(0.85f, 0.85f, 0.9f);
    if (body->type == BodyType::Kinematic) return Color(0.3f, 0.65f, 1.0f);
    return Color(0.4f, 1.0f, 0.55f);
}

void drawColliders(Registry& registry)
{
    SparseSet<RigidBodyComponent>& bodies = registry.getPool<RigidBodyComponent>();

    View<TransformComponent, ColliderComponent> view(registry);
    view.each([&](Entity entity, TransformComponent& transform, ColliderComponent& collider) {
        RigidBodyComponent* body = bodies.contains(entity) ? &bodies.get(entity) : nullptr;
        Color color = colliderColor(body, collider);
        float thickness = g_triggerLog.inside.count(entity) ? 4.0f : 2.0f;

        if (collider.shape == ColliderShape::Circle) {
            Collisions::Circle circle = Collisions::toCircle(transform, collider);
            Renderer::get().addCircleFrame(circle.center, circle.radius, color, thickness);
            return;
        }

        Collisions::Box box = Collisions::toBox(transform, collider);
        Renderer::get().addFrame(
            box.center - box.halfExtents, box.halfExtents * 2, color, thickness
        );
    });
}

void drawContacts(Registry& registry)
{
    for (const ContactRecord& record : PhysicsManager::get().getContacts(registry)) {
        Color color = record.isTrigger ? Color(1.0f, 0.35f, 0.9f) : Color(1.0f, 0.3f, 0.35f);
        Renderer::get().addLine(
            record.contact.point, record.contact.point + record.contact.normal * 30.0f, color, 2.0f
        );
    }
}

void toss(EntityHandle entity)
{
    RigidBodyComponent& body = entity.get<RigidBodyComponent>();
    body.velocity = Vec2(
        Random::rangeFloat(-g_spawn.speedRange, g_spawn.speedRange),
        Random::rangeFloat(g_spawn.liftMin, g_spawn.liftMax)
    );
    body.bounciness = Random::rangeFloat(g_spawn.bouncinessMin, g_spawn.bouncinessMax);
    body.friction = g_spawn.friction;
}

Entity buildScene(Registry& registry)
{
    makeBox(registry, Vec2(0, -380), Vec2(460, 20), BodyType::Static, false);
    makeBox(registry, Vec2(-440, -60), Vec2(20, 300), BodyType::Static, false);
    makeBox(registry, Vec2(440, -60), Vec2(20, 300), BodyType::Static, true);
    makeBox(registry, Vec2(-180, -270), Vec2(120, 20), BodyType::Static, false);

    EntityHandle platform = makeBox(registry, Vec2(0, -130), Vec2(110, 15), BodyType::Kinematic);
    platform.get<RigidBodyComponent>().velocity = Vec2(180.0f, 0.0f);

    makeBox(registry, Vec2(250, -240), Vec2(70, 70), BodyType::Static, false, true);

    toss(makeBox(registry, Vec2(-200, 200), Vec2(25, 25), BodyType::Dynamic));
    toss(makeBox(registry, Vec2(-140, 340), Vec2(30, 18), BodyType::Dynamic));
    toss(makeBox(registry, Vec2(40, 260), Vec2(20, 20), BodyType::Dynamic));
    toss(makeBox(registry, Vec2(300, 300), Vec2(35, 35), BodyType::Dynamic));

    toss(makeCircle(registry, Vec2(-60, 380), 28.0f, BodyType::Dynamic));
    toss(makeCircle(registry, Vec2(120, 180), 34.0f, BodyType::Dynamic));
    toss(makeCircle(registry, Vec2(230, 420), 22.0f, BodyType::Dynamic));

    return platform.getEntity();
}

Entity resetScene(Registry& registry)
{
    std::vector<Entity> toDestroy;
    View<ColliderComponent> view(registry);
    view.each([&](Entity entity, ColliderComponent&) { toDestroy.push_back(entity); });
    for (Entity entity : toDestroy) registry.destroy(entity);

    // Trigger pairs outlive the entities they name, so an exit would fire for dead ids.
    PhysicsWorld& world = PhysicsManager::get().getWorld(registry);
    world.contacts.clear();
    world.triggerPairs.clear();
    world.prevTriggerPairs.clear();
    g_triggerLog.clear();

    return buildScene(registry);
}

enum class QueryMode
{
    Off,
    Point,
    Box,
    Circle,
    Ray,
    RayAll,
    CircleCast,
    CircleCastAll,
    BoxCast,
    BoxCastAll
};

const std::vector<std::string> QUERY_NAMES = {
    "Off",         "Point",           "Box",      "Circle",      "Ray", "Ray All",
    "Circle Cast", "Circle Cast All", "Box Cast", "Box Cast All"
};

const Color QUERY_SHAPE(0.6f, 0.8f, 1.0f);
const Color QUERY_SWEPT(0.35f, 0.5f, 0.75f);
const Color QUERY_HIT(1.0f, 0.45f, 0.2f);

const float CAST_RADIUS = 40.0f;
const Vec2 CAST_HALF_EXTENTS(50.0f, 32.0f);

void panel(Registry& registry, bool& resetClicked, QueryMode& mode)
{
    using namespace UIWidgets;

    PhysicsWorld& world = PhysicsManager::get().getWorld(registry);
    Collisions::ResolveSettings& resolve = world.resolveSettings;

    openWindow("Physics", {.windowLayout = {.width = UISizeSpec::fixed(320.0f)}});

    openContainer({.gap = 8.0f}, {}, "physicsActions");
    resetClicked = button("Reset Scene", {.key = "physicsReset"}).isReleased;
    int selected = static_cast<int>(mode);
    dropdown(QUERY_NAMES, selected, {.key = "queryMode"});
    mode = static_cast<QueryMode>(selected);
    closeContainer();

    if (openSection("World", {.openByDefault = true, .key = "physicsWorld"})) {
        sliderFloat("Gravity X", world.gravity.x, -2000.0f, 2000.0f, {.key = "gravityX"});
        sliderFloat("Gravity Y", world.gravity.y, -2000.0f, 2000.0f, {.key = "gravityY"});
        sliderFloat("Slop", resolve.penetrationSlop, 0.0f, 5.0f, {.key = "slop"});
        sliderFloat("Correction", resolve.correctionPercent, 0.0f, 1.0f, {.key = "correction"});
        sliderFloat("Bounce Thr.", resolve.bounceThreshold, 0.0f, 200.0f, {.key = "bounceThr"});
    }
    closeSection();

    if (openSection("Spawn", {.openByDefault = true, .key = "physicsSpawn"})) {
        sliderFloat("Speed X", g_spawn.speedRange, 0.0f, 800.0f, {.key = "speedX"});
        sliderFloat("Lift Min", g_spawn.liftMin, -800.0f, 800.0f, {.key = "liftMin"});
        sliderFloat("Lift Max", g_spawn.liftMax, -800.0f, 800.0f, {.key = "liftMax"});
        sliderFloat("Bounce Min", g_spawn.bouncinessMin, 0.0f, 1.0f, {.key = "bounceMin"});
        sliderFloat("Bounce Max", g_spawn.bouncinessMax, 0.0f, 1.0f, {.key = "bounceMax"});
        sliderFloat("Friction", g_spawn.friction, 0.0f, 1.0f, {.key = "friction"});
    }
    closeSection();

    if (openSection("Triggers", {.openByDefault = true, .key = "physicsTriggers"})) {
        text(
            std::format(
                "enters {}   exits {}   inside {}", g_triggerLog.enterCount, g_triggerLog.exitCount,
                g_triggerLog.inside.size()
            )
        );
        for (const std::string& line : g_triggerLog.lines) text(line);
    }
    closeSection();

    closeWindow();
}

void drawMarker(Vec2 point, Color color)
{
    Renderer::get().addCircleFrame(point, 6.0f, color, 2.0f);
}

void drawContact(const Collisions::Contact& contact)
{
    drawMarker(contact.point, QUERY_HIT);
    Renderer::get().addLine(
        contact.point, contact.point + contact.normal * contact.depth, QUERY_HIT, 2.0f
    );
}

void drawHit(const Collisions::RayHit& hit)
{
    drawMarker(hit.point, QUERY_HIT);
    Renderer::get().addLine(hit.point, hit.point + hit.normal * 30.0f, QUERY_HIT, 2.0f);
}

void outlineEntity(Registry& registry, Entity entity)
{
    if (entity == NULL_ENTITY) return;

    SparseSet<ColliderComponent>& colliders = registry.getPool<ColliderComponent>();
    if (!colliders.contains(entity)) return;

    TransformComponent& transform = registry.getPool<TransformComponent>().get(entity);
    ColliderComponent& collider = colliders.get(entity);

    if (collider.shape == ColliderShape::Circle) {
        Collisions::Circle circle = Collisions::toCircle(transform, collider);
        Renderer::get().addCircleFrame(circle.center, circle.radius + 6.0f, QUERY_HIT, 3.0f);
        return;
    }

    Collisions::Box box = Collisions::toBox(transform, collider);
    Vec2 padded = box.halfExtents + Vec2(6.0f, 6.0f);
    Renderer::get().addFrame(box.center - padded, padded * 2, QUERY_HIT, 3.0f);
}

Vec2 castStop(const Collisions::Ray& path, float distance)
{
    return path.start + (path.end - path.start).normalized() * distance;
}

void drawCircleCastHit(
    Registry& registry, const Collisions::Ray& path, const Collisions::RayHit& hit
)
{
    Renderer::get().addCircleFrame(castStop(path, hit.distance), CAST_RADIUS, QUERY_HIT, 2.0f);
    outlineEntity(registry, hit.entity);
    drawHit(hit);
}

void drawBoxCastHit(Registry& registry, const Collisions::Ray& path, const Collisions::RayHit& hit)
{
    Renderer::get().addFrame(
        castStop(path, hit.distance) - CAST_HALF_EXTENTS, CAST_HALF_EXTENTS * 2, QUERY_HIT, 2.0f
    );
    outlineEntity(registry, hit.entity);
    drawHit(hit);
}

void runQuery(Registry& registry, QueryMode mode, Vec2 mouse)
{
    PhysicsManager& physics = PhysicsManager::get();

    switch (mode) {
    case QueryMode::Off: return;

    case QueryMode::Point: {
        drawMarker(mouse, QUERY_SHAPE);
        for (Entity entity : physics.pointTest(registry, mouse)) outlineEntity(registry, entity);
        return;
    }

    case QueryMode::Box: {
        Collisions::Box box {.center = mouse, .halfExtents = Vec2(60.0f, 40.0f)};
        Renderer::get().addFrame(
            box.center - box.halfExtents, box.halfExtents * 2, QUERY_SHAPE, 2.0f
        );
        for (const Collisions::Contact& contact : physics.boxTest(registry, box)) {
            outlineEntity(registry, contact.entity);
            drawContact(contact);
        }
        return;
    }

    case QueryMode::Circle: {
        Collisions::Circle circle {.center = mouse, .radius = 50.0f};
        Renderer::get().addCircleFrame(circle.center, circle.radius, QUERY_SHAPE, 2.0f);
        for (const Collisions::Contact& contact : physics.circleTest(registry, circle)) {
            outlineEntity(registry, contact.entity);
            drawContact(contact);
        }
        return;
    }

    case QueryMode::Ray: {
        Collisions::Ray ray {.start = VEC2_ZERO, .end = mouse};
        Renderer::get().addLine(ray.start, ray.end, QUERY_SHAPE, 2.0f);

        Collisions::RayHit hit = physics.rayTest(registry, ray);
        if (hit.isHit) {
            outlineEntity(registry, hit.entity);
            drawHit(hit);
        }
        return;
    }

    case QueryMode::RayAll: {
        Collisions::Ray ray {.start = VEC2_ZERO, .end = mouse};
        Renderer::get().addLine(ray.start, ray.end, QUERY_SHAPE, 2.0f);

        for (const Collisions::RayHit& hit : physics.rayTestAll(registry, ray)) {
            outlineEntity(registry, hit.entity);
            drawHit(hit);
        }
        return;
    }

    case QueryMode::CircleCast:
    case QueryMode::CircleCastAll: {
        Collisions::Ray path {.start = VEC2_ZERO, .end = mouse};
        Renderer::get().addLine(path.start, path.end, QUERY_SHAPE, 2.0f);
        Renderer::get().addCircleFrame(path.start, CAST_RADIUS, QUERY_SHAPE, 2.0f);
        Renderer::get().addCircleFrame(path.end, CAST_RADIUS, QUERY_SWEPT, 1.0f);

        if (mode == QueryMode::CircleCast) {
            Collisions::RayHit hit = physics.circleCast(registry, path, CAST_RADIUS);
            if (hit.isHit) drawCircleCastHit(registry, path, hit);
            return;
        }
        for (const Collisions::RayHit& hit : physics.circleCastAll(registry, path, CAST_RADIUS)) {
            drawCircleCastHit(registry, path, hit);
        }
        return;
    }

    case QueryMode::BoxCast:
    case QueryMode::BoxCastAll: {
        Collisions::Ray path {.start = VEC2_ZERO, .end = mouse};
        Renderer::get().addLine(path.start, path.end, QUERY_SHAPE, 2.0f);
        Renderer::get().addFrame(
            path.start - CAST_HALF_EXTENTS, CAST_HALF_EXTENTS * 2, QUERY_SHAPE, 2.0f
        );
        Renderer::get().addFrame(
            path.end - CAST_HALF_EXTENTS, CAST_HALF_EXTENTS * 2, QUERY_SWEPT, 1.0f
        );

        if (mode == QueryMode::BoxCast) {
            Collisions::RayHit hit = physics.boxCast(registry, path, CAST_HALF_EXTENTS);
            if (hit.isHit) drawBoxCastHit(registry, path, hit);
            return;
        }
        for (const Collisions::RayHit& hit :
             physics.boxCastAll(registry, path, CAST_HALF_EXTENTS)) {
            drawBoxCastHit(registry, path, hit);
        }
        return;
    }
    }
}

}   // namespace

int physics_test()
{
    IdType windowId = WindowManager::get().createWindow(
        {.width = 1000,
         .height = 800,
         .title = "Physics Test",
         .creationHints = WindowFlags::Transparent}
    );

    Registry registry;
    EntityHandle camera = registry.create();
    camera.emplace<TransformComponent>();
    camera.emplace<CameraComponent>().windowId = windowId;
    camera.get<CameraComponent>().orthoSize = 500;

    Renderer::get().init();
    TextRenderer::get().init();
    UIRenderer::get().init();

    Input::get().addAxis("Horizontal", {KeyCode::D, KeyCode::A});
    Input::get().addAxis("Vertical", {KeyCode::W, KeyCode::S});
    Input::get().addAxis("Zoom", {KeyCode::E, KeyCode::Q});

    PhysicsWorld& world = registry.getContext<PhysicsWorld>();
    world.gravity = Vec2(0.0f, -900.0f);
    world.resolveSettings.bounceThreshold = 40.0f;

    g_triggerLog.registry = &registry;
    world.onTriggerEnter.connect<&TriggerLog::onEnter>(&g_triggerLog);
    world.onTriggerExit.connect<&TriggerLog::onExit>(&g_triggerLog);

    Entity platform = buildScene(registry);

    auto onFixedUpdate = [&](float dt) {
        RigidBodyComponent& platformBody = registry.getPool<RigidBodyComponent>().get(platform);
        float platformX = registry.getPool<TransformComponent>().get(platform).position.x;
        if (platformX > 250 && platformBody.velocity.x > 0) platformBody.velocity.x *= -1;
        if (platformX < -250 && platformBody.velocity.x < 0) platformBody.velocity.x *= -1;

        PhysicsManager::get().step(registry, dt);
    };

    auto onWindowUpdate = [&](IdType winId, float dt) {
        TransformComponent& transform = camera.get<TransformComponent>();
        CameraComponent& cam = camera.get<CameraComponent>();
        transform.position.x += Input::get().getAxis("Horizontal") * dt * cam.orthoSize;
        transform.position.y += Input::get().getAxis("Vertical") * dt * cam.orthoSize;
        cam.orthoSize -= Input::get().getAxis("Zoom") * dt * cam.orthoSize;
    };

    QueryMode queryMode = QueryMode::Off;

    auto onWindowRender = [&](IdType winId, float dt) {
        Renderer::get().clearColor(Color(0.09f, 0.09f, 0.13f));

        Renderer::get().beginScene();
        drawColliders(registry);
        drawContacts(registry);
        runQuery(registry, queryMode, ViewContext::get().getMouseWorldPos());
        Renderer::get().endScene();

        bool resetClicked = false;
        UIWidgets::clear();
        panel(registry, resetClicked, queryMode);
        UIManager::get().draw();

        if (resetClicked) platform = resetScene(registry);
    };

    Application app(registry);
    app.onFixedUpdate().bind(&onFixedUpdate);
    app.onWindowUpdate().bind(&onWindowUpdate);
    app.onWindowRender().bind(&onWindowRender);
    app.run();

    return 0;
}
