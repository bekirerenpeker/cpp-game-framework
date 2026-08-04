#include "EngineInclude.hpp"
#include "test_funcs.hpp"
#include <glad/glad.h>

using namespace Engine;

// Draws a rule-tile map re-sampled from 3D Perlin noise every frame (time on the z
// axis), so TilemapRenderer keeps re-resolving each tile's region/rotation from its
// live 8-neighbor connectivity. WASD/QE pan and zoom; V toggles wireframe.
int tilemap_test()
{
    LOG_INFO("================= TILEMAP RENDER TEST =================");

    IdType windowId =
        WindowManager::get().createWindow({1000, 800, "Tilemap Test", WindowFlags::Transparent});

    Registry registry;
    EntityHandle camera = registry.create();
    camera.emplace<TransformComponent>();
    camera.emplace<CameraComponent>().windowId = windowId;
    camera.get<CameraComponent>().orthoSize = 48;

    // Partitioning lives on the atlas. Slice the whole sheet into 8x8 regions
    // "tile0", "tile1", ... (transparent cells are skipped); the count is the
    // number of regions produced.
    Tileset tileset("game/assets/images/ruletile-47-kingspigs-tileset.png");
    int tileCount = static_cast<int>(tileset.getAtlas().fromCellSize("tile", 32, 32).size());

    if (tileCount < 1) {
        LOG_ERROR("      ERROR: tileset produced too few tiles (is TilesetFloorB.png present?)");
        return 1;
    }

    // "47-tile" is the built-in blob template; it's generated on first use.
    uint16_t ruleTileId = tileset.createRuleTile("ruleTile", "tile", 0, tileCount, "47-tile");

    EntityHandle tilemapEntity = registry.create();
    TilemapManager::get().setTileset(tilemapEntity.emplace<TilemapComponent>(), &tileset);

    int mapWidth = 250;
    int mapHeight = 250;
    float frequency = 0.1f;
    float scrollSpeed = 0.32f;

    Renderer::get().init(10000);
    TilemapRenderer::get().init(nullptr, mapWidth * mapHeight);

    Input::get().addAxis("Horizontal", {KeyCode::D, KeyCode::A, KeyCode::Right, KeyCode::Left});
    Input::get().addAxis("Vertical", {KeyCode::W, KeyCode::S, KeyCode::Up, KeyCode::Down});
    Input::get().addAxis("Zoom", {KeyCode::E, KeyCode::Q});

    bool wireframe = false;

    auto onFrame = [&](float dt) {
        float z = Time::get().currTime() * scrollSpeed;
        TilemapComponent& tilemap = tilemapEntity.get<TilemapComponent>();
        for (int y = 0; y < mapHeight; y++) {
            for (int x = 0; x < mapWidth; x++) {
                bool solid = Math::perlin3D(x * frequency, y * frequency, z) >= 0.5f;
                uint16_t id = solid ? ruleTileId : 0;
                TilemapManager::get().setAt(tilemap, x - mapWidth / 2, y - mapHeight / 2, {id});
            }
        }
    };

    auto onWindowUpdate = [&](IdType id, float dt) {
        View<TransformComponent, CameraComponent> camView(registry);
        for (const auto& [ent, trans, cam] : camView) {
            if (cam.windowId != id) continue;
            trans.position.x += Input::get().getAxis("Horizontal") * dt * cam.orthoSize;
            trans.position.y += Input::get().getAxis("Vertical") * dt * cam.orthoSize;
            cam.orthoSize -= Input::get().getAxis("Zoom") * dt * cam.orthoSize;
        }
        if (Input::get().keyPressed(KeyCode::V)) wireframe = !wireframe;
    };

    auto onWindowRender = [&](IdType id, float dt) {
        Renderer::get().beginScene();
        Renderer::get().clearColor(Color(0.1f, 0.1f, 0.15f, 1.0f));

        glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
        TilemapRenderer::get().render(registry);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        Renderer::get().endScene();
    };

    Application app(registry);
    app.onFrame().bind(&onFrame);
    app.onWindowUpdate().bind(&onWindowUpdate);
    app.onWindowRender().bind(&onWindowRender);
    app.run();

    LOG_INFO("==========================================================\n");
    return 0;
}
