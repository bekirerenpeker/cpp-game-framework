#include "EngineInclude.hpp"
#include "test_funcs.hpp"
#include <glad/glad.h>

using namespace Engine;

// Draws a static Perlin-generated tilemap made entirely of animated tiles. The
// layout is placed once; three vertical bands use animated tiles that share a
// frame count but differ in frame time, so the speeds sit side by side. The
// TilemapRenderer re-emits only the animated tiles each frame. WASD/QE pan and
// zoom; press V to toggle wireframe.
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

    // 16x16 px tiles. fromCellSize does not skip empty cells yet (known), so
    // every grid cell becomes a named tile "tile0", "tile1", ...
    Tileset tileset("game/assets/images/TilesetFloorB.png");
    tileset.fromCellSize("tile", 16, 16);

    if (tileset.getTileId("tile0") == 0) {
        LOG_ERROR("      ERROR: tileset produced no tiles (is TilesetFloorB.png present?)");
        return 1;
    }

    // Named tiles run tile0..tile(N-1) with ids 1..N. Many atlas cells are
    // transparent, so cycling ids across the placed tiles keeps them visible.
    int tileCount = 0;
    while (tileset.getTileId("tile" + std::to_string(tileCount)) != 0) tileCount++;

    int mapWidth = 200;
    int mapHeight = 200;
    // Scales the noise sample coords; too low and the whole map crosses the
    // threshold at once, too high and the pattern turns to static.
    float frequency = 0.1f;

    // Frames pulled from consecutive (non-empty) atlas tiles, wrapping at the end.
    auto frameKeys = [&](int base, int count) {
        std::vector<std::string> keys;
        for (int i = 0; i < count; i++) {
            keys.push_back("tile" + std::to_string((base + i) % tileCount));
        }
        return keys;
    };

    // Three animated tiles with the same frame count but different frame times.
    uint16_t animTiles[] = {
        tileset.createAnimatedTile("animSlow", frameKeys(0, 4), 0.5f),
        tileset.createAnimatedTile("animMed", frameKeys(4, 4), 0.2f),
        tileset.createAnimatedTile("animFast", frameKeys(8, 4), 0.07f),
    };

    TilemapComponent tilemap;
    TilemapManager::get().setTileset(tilemap, &tileset);

    // Place the map once (it never changes after this). Perlin decides where a
    // tile goes; three vertical bands choose which animation, so the differing
    // frame rates sit side by side for comparison.
    for (int y = 0; y < mapHeight; y++) {
        for (int x = 0; x < mapWidth; x++) {
            if (Math::perlin2D(x * frequency, y * frequency) < 0.5f) continue;
            uint16_t id = animTiles[(x * 3) / mapWidth];
            TilemapManager::get().setAt(tilemap, x - mapWidth / 2, y - mapHeight / 2, {id, 0});
        }
    }

    GlShader tilemapShader("game/assets/shaders/TilemapShader.glsl");
    GlShader ppShader("game/assets/shaders/PostProcessingShader.glsl");
    Renderer::get().init(10000, &ppShader);
    TilemapRenderer::get().init(&tilemapShader, mapWidth * mapHeight);

    Input::get().addAxis("Horizontal", {KeyCode::D, KeyCode::A, KeyCode::Right, KeyCode::Left});
    Input::get().addAxis("Vertical", {KeyCode::W, KeyCode::S, KeyCode::Up, KeyCode::Down});
    Input::get().addAxis("Zoom", {KeyCode::E, KeyCode::Q});

    bool loggedCount = false;
    bool wireframe = false;
    std::vector<IdType> windowsToClose;
    while (WindowManager::get().anyWindowOpen()) {
        windowsToClose.clear();
        Time::get().update();
        float dt = Time::get().deltaTime();

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

            Renderer::get().beginPass();
            Renderer::get().clearColor(Color(0.1f, 0.1f, 0.15f, 1.0f));

            glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
            TilemapRenderer::get().render(tilemap, id, Renderer::get().getViewProjMat());

            if (!loggedCount) {
                LOG_INFO(
                    "      total indices = {}", TilemapRenderer::get().totalIndexCount(tilemap)
                );
                loggedCount = true;
            }

            Renderer::get().beginPass();
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);   // keep the post-process quad solid
            Renderer::get().setShader(&ppShader);
            Renderer::get().drawToWindow();

            window->swapBuffers();
        }

        GlfwContext::pollEvents();
        for (const auto& id : windowsToClose) WindowManager::get().closeWindow(id);
    }

    LOG_INFO("==========================================================\n");
    return 0;
}
