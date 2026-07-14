#include "EngineInclude.hpp"
#include "test_funcs.hpp"
#include <glad/glad.h>

using namespace Engine;

// Draws an animated tilemap through the TilemapRenderer. Each frame it rewrites a
// mapWidth x mapHeight grid from 3D Perlin noise (time on the z axis, so the
// pattern scrolls) and renders it via the batch renderer. WASD/QE pan and zoom;
// press V to toggle wireframe.
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
    // How fast the noise field scrolls along its 3rd axis per second.
    float scrollSpeed = 0.5f;

    TilemapComponent tilemap;
    TilemapManager::get().setTileset(tilemap, &tileset);

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

        // Scroll the map by sampling 3D Perlin with time as the z axis; a tile is
        // placed where the field crosses the threshold and cleared where it does
        // not, so the pattern animates. Ids are position-based so a cell keeps a
        // stable texture as blobs move through it.
        float z = Time::get().currTime() * scrollSpeed;
        for (int y = 0; y < mapHeight; y++) {
            for (int x = 0; x < mapWidth; x++) {
                bool solid = Math::perlin3D(x * frequency, y * frequency, z) >= 0.5f;
                uint16_t id = solid ? static_cast<uint16_t>(1 + (x + y * mapWidth) % tileCount) : 0;
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

            Renderer::get().beginPass();
            Renderer::get().clearColor(Color(0.1f, 0.1f, 0.15f, 1.0f));

            glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
            TilemapRenderer::get().render(tilemap, Renderer::get().getViewProjMat());

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
