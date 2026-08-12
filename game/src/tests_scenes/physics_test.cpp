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
    int bodyCount = 24;
    float speedRange = 280.0f;
    float liftMin = -200.0f;
    float liftMax = 320.0f;
    float bouncinessMin = 0.0f;
    float bouncinessMax = 0.9f;
    float friction = 0.3f;
};

struct RoomSettings
{
    int width = 30;
    int height = 24;
};

const float TILE_SIZE = 40.0f;
constexpr int MAX_ROOM_TILES = 64 * 48;

// The kinematic platform sweeps this band, so no ledge may generate into it.
const float PLATFORM_BAND_MIN = -175.0f;
const float PLATFORM_BAND_MAX = -100.0f;

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
RoomSettings g_room;
TriggerLog g_triggerLog;

// The room is centred on the origin so resizing grows it in every direction at once.
Vec2 roomOrigin() { return Vec2(g_room.width * TILE_SIZE, g_room.height * TILE_SIZE) * -0.5f; }

Vec2 spawnMin()
{
    Vec2 origin = roomOrigin();
    return origin + Vec2(TILE_SIZE * 2.0f, TILE_SIZE * (g_room.height - 6));
}

Vec2 spawnMax()
{
    Vec2 origin = roomOrigin();
    return origin + Vec2(TILE_SIZE * (g_room.width - 2), TILE_SIZE * (g_room.height - 2));
}

float interiorHalfWidth() { return -roomOrigin().x - TILE_SIZE; }

// The platform shrinks and turns around early enough to keep clear of the walls, whatever the
// room is sized to -- a kinematic body that reaches one has its velocity zeroed and stays put.
float platformHalfWidth() { return Math::min(110.0f, interiorHalfWidth() * 0.3f); }
float platformRange() { return Math::max(interiorHalfWidth() - platformHalfWidth(), 0.0f); }

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
        // The tilemap draws itself through TilemapRenderer.
        if (collider.shape == ColliderShape::Tilemap) return;

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

// Walls, ceiling and a scatter of ledges, all as solid tiles -- the room the bodies live in is
// the tilemap, so the swept axis-separated path in `integrate` is what holds everything up.
void buildRoom(EntityHandle room, uint16_t tileId)
{
    Vec2 origin = roomOrigin();
    room.get<TransformComponent>().position = Vec3(origin.x, origin.y, 0.0f);

    TilemapComponent& tilemap = room.get<TilemapComponent>();
    const TilemapManager& tiles = TilemapManager::get();
    tiles.clear(tilemap);

    auto fill = [&](int x0, int y0, int x1, int y1) {
        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) tiles.setAt(tilemap, x, y, {tileId});
        }
    };

    fill(0, 0, g_room.width - 1, 0);
    fill(0, g_room.height - 1, g_room.width - 1, g_room.height - 1);
    fill(0, 0, 0, g_room.height - 1);
    fill(g_room.width - 1, 0, g_room.width - 1, g_room.height - 1);

    // A body spawned inside solid tiles has no contact normal to push out along and drops
    // straight through them, so nothing generates into the spawn band or the platform's lane.
    float spawnFloor = spawnMin().y;
    auto rowIsClear = [&](int row) {
        float low = origin.y + row * TILE_SIZE;
        float high = low + TILE_SIZE;
        if (high > spawnFloor - TILE_SIZE) return false;
        return low >= PLATFORM_BAND_MAX || high <= PLATFORM_BAND_MIN;
    };

    int ledgeCount = g_room.width * g_room.height / 90;
    for (int i = 0; i < ledgeCount; i++) {
        int row = Random::rangeInt(2, g_room.height - 2);
        if (!rowIsClear(row)) continue;

        int length = Random::rangeInt(3, 9);
        int startX = Random::rangeInt(1, g_room.width - 1);
        int endX = startX + length - 1;
        fill(startX, row, endX < g_room.width - 1 ? endX : g_room.width - 2, row);
    }

    for (int i = 0; i < ledgeCount / 2; i++) {
        int row = Random::rangeInt(2, g_room.height - 2);
        if (!rowIsClear(row)) continue;
        tiles.setAt(tilemap, Random::rangeInt(1, g_room.width - 1), row, {tileId});
    }
}

Entity buildScene(Registry& registry)
{
    EntityHandle platform =
        makeBox(registry, Vec2(0, -130), Vec2(platformHalfWidth(), 15), BodyType::Kinematic);
    platform.get<RigidBodyComponent>().velocity = Vec2(180.0f, 0.0f);

    float triggerHalf = Math::min(70.0f, interiorHalfWidth() * 0.2f);
    Vec2 triggerPos(interiorHalfWidth() * 0.45f, roomOrigin().y + TILE_SIZE + triggerHalf);
    makeBox(registry, triggerPos, Vec2(triggerHalf, triggerHalf), BodyType::Static, false, true);

    Vec2 low = spawnMin(), high = spawnMax();
    for (int i = 0; i < g_spawn.bodyCount; i++) {
        Vec2 pos(Random::rangeFloat(low.x, high.x), Random::rangeFloat(low.y, high.y));

        if (Random::float01() < 0.5f) {
            toss(makeCircle(registry, pos, Random::rangeFloat(12.0f, 32.0f), BodyType::Dynamic));
            continue;
        }
        Vec2 halfExtents(Random::rangeFloat(12.0f, 35.0f), Random::rangeFloat(12.0f, 35.0f));
        toss(makeBox(registry, pos, halfExtents, BodyType::Dynamic));
    }

    return platform.getEntity();
}

Entity resetScene(Registry& registry)
{
    std::vector<Entity> toDestroy;
    View<ColliderComponent> view(registry);
    view.each([&](Entity entity, ColliderComponent& collider) {
        if (collider.shape == ColliderShape::Tilemap) return;
        toDestroy.push_back(entity);
    });
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

void panel(Registry& registry, bool& resetClicked, bool& roomChanged, QueryMode& mode)
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

    if (openSection("Room", {.openByDefault = true, .key = "physicsRoom"})) {
        roomChanged |= sliderInt("Width", g_room.width, 10, 65, {.key = "roomWidth"}).isReleased;
        roomChanged |= sliderInt("Height", g_room.height, 10, 49, {.key = "roomHeight"}).isReleased;
    }
    closeSection();

    if (openSection("World", {.openByDefault = true, .key = "physicsWorld"})) {
        sliderFloat("Gravity X", world.gravity.x, -2000.0f, 2000.0f, {.key = "gravityX"});
        sliderFloat("Gravity Y", world.gravity.y, -2000.0f, 2000.0f, {.key = "gravityY"});
        sliderFloat("Slop", resolve.penetrationSlop, 0.0f, 5.0f, {.key = "slop"});
        sliderFloat("Correction", resolve.correctionPercent, 0.0f, 1.0f, {.key = "correction"});
        sliderFloat("Bounce Thr.", resolve.bounceThreshold, 0.0f, 200.0f, {.key = "bounceThr"});
    }
    closeSection();

    if (openSection("Spawn", {.openByDefault = true, .key = "physicsSpawn"})) {
        sliderInt("Bodies", g_spawn.bodyCount, 1, 121, {.key = "bodyCount"});
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

    // A tilemap has no single outline worth drawing; the hit marker already says where.
    if (collider.shape == ColliderShape::Tilemap) return;

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
    camera.get<CameraComponent>().orthoSize = 540;

    Renderer::get().init();
    TextRenderer::get().init();
    UIRenderer::get().init();
    TilemapRenderer::get().init(nullptr, MAX_ROOM_TILES);

    Tileset tileset(FileManager::get().gameAsset("images/ruletile-47-kingspigs-tileset.png"));
    int tileCount = static_cast<int>(tileset.getAtlas().fromCellSize("tile", 32, 32).size());
    if (tileCount < 1) {
        LOG_ERROR("tileset produced no tiles; is ruletile-47-kingspigs-tileset.png present?");
        return 1;
    }

    uint16_t roomTileId = tileset.createRuleTile("room", "tile", 0, tileCount, "47-tile");
    tileset.setTileSolid(roomTileId, true);

    Input::get().addAxis("Horizontal", {KeyCode::D, KeyCode::A});
    Input::get().addAxis("Vertical", {KeyCode::W, KeyCode::S});
    Input::get().addAxis("Zoom", {KeyCode::E, KeyCode::Q});

    PhysicsWorld& world = registry.getContext<PhysicsWorld>();
    world.gravity = Vec2(0.0f, -900.0f);
    world.resolveSettings.bounceThreshold = 40.0f;

    g_triggerLog.registry = &registry;
    world.onTriggerEnter.connect<&TriggerLog::onEnter>(&g_triggerLog);
    world.onTriggerExit.connect<&TriggerLog::onExit>(&g_triggerLog);

    EntityHandle room = registry.create();
    room.emplace<TransformComponent>().scale = Vec2(TILE_SIZE, TILE_SIZE);
    room.emplace<ColliderComponent>().shape = ColliderShape::Tilemap;
    TilemapManager::get().setTileset(room.emplace<TilemapComponent>(), &tileset);
    buildRoom(room, roomTileId);

    Entity platform = buildScene(registry);

    auto onFixedUpdate = [&](float dt) {
        RigidBodyComponent& platformBody = registry.getPool<RigidBodyComponent>().get(platform);
        float platformX = registry.getPool<TransformComponent>().get(platform).position.x;
        float range = platformRange();
        if (platformX > range && platformBody.velocity.x > 0) platformBody.velocity.x *= -1;
        if (platformX < -range && platformBody.velocity.x < 0) platformBody.velocity.x *= -1;

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
        TilemapRenderer::get().render(registry);
        drawColliders(registry);
        drawContacts(registry);
        runQuery(registry, queryMode, ViewContext::get().getMouseWorldPos());
        Renderer::get().endScene();

        bool resetClicked = false, roomChanged = false;
        UIWidgets::clear();
        panel(registry, resetClicked, roomChanged, queryMode);
        UIManager::get().draw();

        // Resizing moves the walls out from under whatever is resting on them, so the bodies
        // are respawned with them.
        if (roomChanged) buildRoom(room, roomTileId);
        if (roomChanged || resetClicked) platform = resetScene(registry);
    };

    Application app(registry);
    app.onFixedUpdate().bind(&onFixedUpdate);
    app.onWindowUpdate().bind(&onWindowUpdate);
    app.onWindowRender().bind(&onWindowRender);
    app.run();

    return 0;
}
