#include "EngineInclude.hpp"
#include "test_funcs.hpp"
#include <glad/glad.h>

using namespace Engine;

// Draws a rule-tile map whose contents are re-sampled from 3D Perlin noise every
// frame (time on the z axis, so the pattern scrolls). A cell is set to the rule
// tile where the field crosses the threshold and cleared elsewhere; since the
// field keeps shifting, TilemapRenderer continuously re-resolves each rule
// tile's region + rotation from its live 8-neighbor connectivity on every
// rebake. WASD/QE pan and zoom; V toggles wireframe.
int tilemap_test()
{
    LOG_INFO("================= TILEMAP RENDER TEST =================");

    IdType windowId =
        WindowManager::get().createWindow({1000, 800, "Tilemap Test", WindowFlags::Transparent});

    Registry registry;
    EntityHandle camera = registry.create();
    camera.emplace<TransformComponent>().position = Vec3(32, 20, 0);
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

    TilemapComponent tilemap;
    TilemapManager::get().setTileset(tilemap, &tileset);

    int mapWidth = 200;
    int mapHeight = 200;
    // Scales the noise sample coords; too low and the whole map crosses the
    // threshold at once, too high and the pattern turns to static.
    float frequency = 0.1f;
    // How fast the noise field scrolls along its 3rd axis per second.
    float scrollSpeed = 0.5f;

    GlShader tilemapShader("game/assets/shaders/TilemapShader.glsl");
    GlShader quadShader("game/assets/shaders/QuadShader.glsl");
    Renderer::get().init(10000, &quadShader);
    TilemapRenderer::get().init(&tilemapShader, mapWidth * mapHeight);

    Input::get().addAxis("Horizontal", {KeyCode::D, KeyCode::A, KeyCode::Right, KeyCode::Left});
    Input::get().addAxis("Vertical", {KeyCode::W, KeyCode::S, KeyCode::Up, KeyCode::Down});
    Input::get().addAxis("Zoom", {KeyCode::E, KeyCode::Q});

    bool wireframe = false;
    std::vector<IdType> windowsToClose;
    while (WindowManager::get().anyWindowOpen()) {
        windowsToClose.clear();
        Time::get().update();
        float dt = Time::get().deltaTime();

        // Scroll the map by sampling 3D Perlin with time as the z axis; a tile is
        // placed where the field crosses the threshold and cleared where it does
        // not, so the pattern animates and the rule tile keeps re-picking its
        // region/rotation as neighbors change.
        float z = Time::get().currTime() * scrollSpeed;
        for (int y = 0; y < mapHeight; y++) {
            for (int x = 0; x < mapWidth; x++) {
                bool solid = Math::perlin3D(x * frequency, y * frequency, z) >= 0.5f;
                uint16_t id = solid ? ruleTileId : 0;
                TilemapManager::get().setAt(tilemap, x - mapWidth / 2, y - mapHeight / 2, {id, 0});
            }
        }

        for (auto& [id, window] : WindowManager::get().getAllWindows()) {
            Input::get().update(id);

            View<TransformComponent, CameraComponent> camView(registry);
            for (const auto& [ent, trans, cam] : camView) {
                if (cam.windowId != id) continue;
                trans.position.x += Input::get().getAxis("Horizontal") * dt * cam.orthoSize;
                trans.position.y += Input::get().getAxis("Vertical") * dt * cam.orthoSize;
                cam.orthoSize -= Input::get().getAxis("Zoom") * dt * cam.orthoSize;
            }

            if (Input::get().keyPressed(KeyCode::Escape) || !window->isOpen()) {
                windowsToClose.push_back(id);
            }
            if (Input::get().keyPressed(KeyCode::V)) wireframe = !wireframe;

            Renderer::get().setRenderWindowId(id);
            Renderer::get().setViewProjMat(registry);

            Renderer::get().beginScene();
            Renderer::get().clearColor(Color(0.1f, 0.1f, 0.15f, 1.0f));

            glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
            TilemapRenderer::get().render(tilemap, id, Renderer::get().getViewProjMat());
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            Renderer::get().endScene();

            window->swapBuffers();
        }

        GlfwContext::pollEvents();
        for (const auto& id : windowsToClose) WindowManager::get().closeWindow(id);
    }

    LOG_INFO("==========================================================\n");
    return 0;
}
