#include "EngineInclude.hpp"
#include "physics/PhysicsManager.hpp"
#include "test_funcs.hpp"

using namespace Engine;

namespace {

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

        if (collider.shape == ColliderShape::Circle) {
            Collisions::Circle circle = Collisions::toCircle(transform, collider);
            Renderer::get().addCircleFrame(circle.center, circle.radius, color, 2.0f);
            return;
        }

        Collisions::Box box = Collisions::toBox(transform, collider);
        Renderer::get().addFrame(box.center - box.halfExtents, box.halfExtents * 2, color, 2.0f);
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

void toss(EntityHandle entity, float bounciness)
{
    RigidBodyComponent& body = entity.get<RigidBodyComponent>();
    body.velocity = Vec2(Random::rangeFloat(-280.0f, 280.0f), Random::rangeFloat(-200.0f, 320.0f));
    body.bounciness = bounciness;
    body.friction = 0.3f;
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

    toss(makeBox(registry, Vec2(-200, 200), Vec2(25, 25), BodyType::Dynamic), 0.0f);
    toss(makeBox(registry, Vec2(-140, 340), Vec2(30, 18), BodyType::Dynamic), 0.3f);
    toss(makeBox(registry, Vec2(40, 260), Vec2(20, 20), BodyType::Dynamic), 0.6f);
    toss(makeBox(registry, Vec2(300, 300), Vec2(35, 35), BodyType::Dynamic), 0.9f);

    toss(makeCircle(registry, Vec2(-60, 380), 28.0f, BodyType::Dynamic), 0.2f);
    toss(makeCircle(registry, Vec2(120, 180), 34.0f, BodyType::Dynamic), 0.55f);
    toss(makeCircle(registry, Vec2(230, 420), 22.0f, BodyType::Dynamic), 0.85f);

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

    return buildScene(registry);
}

void resetButton(bool& wasClicked)
{
    using namespace UIWidgets;

    openContainer(
        {.width = UISizeSpec::grow(), .height = UISizeSpec::grow(), .padding = UIEdges(12.0f)}, {},
        "physicsToolbar"
    );
    wasClicked = button("Reset Scene", {.key = "physicsReset"}).isReleased;
    closeContainer();
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

    auto onWindowRender = [&](IdType winId, float dt) {
        Renderer::get().clearColor(Color(0.09f, 0.09f, 0.13f));

        Renderer::get().beginScene();
        drawColliders(registry);
        drawContacts(registry);
        Renderer::get().endScene();

        bool resetClicked = false;
        UIWidgets::clear();
        resetButton(resetClicked);
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
