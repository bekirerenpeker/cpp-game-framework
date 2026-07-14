#include "EngineInclude.hpp"
#include "graphics/tilemap/TilemapInclude.hpp"
#include "graphics/tilemap/Tileset.hpp"
#include "test_funcs.hpp"

using namespace Engine;

// Draws a tilemap through the TilemapRenderer. Builds a tileset, writes a few
// tiles across chunk boundaries, and renders them each frame into the window's
// render target via the batch renderer.
int tilemap_test()
{
    LOG_INFO("================= TILEMAP RENDER TEST =================");

    IdType windowId =
        WindowManager::get().createWindow({800, 600, "Tilemap Test", WindowFlags::Transparent});

    Registry registry;
    EntityHandle camera = registry.create();
    camera.emplace<TransformComponent>().position = Vec3(16, 16, 0);
    camera.emplace<CameraComponent>().windowId = windowId;
    camera.get<CameraComponent>().orthoSize = 40;

    // 16x16 px tiles. fromCellSize does not skip empty cells yet (known), so
    // every grid cell becomes a named tile "tile0", "tile1", ...
    Tileset tileset("game/assets/images/TilesetFloorB.png");
    tileset.fromCellSize("tile", 16, 16);

    uint16_t tileId = tileset.getTileId("tile0");
    if (tileId == 0) {
        LOG_ERROR("      ERROR: tileset produced no tiles (is TilesetFloorB.png present?)");
        return 1;
    }

    TilemapComponent tilemap;
    TilemapManager::get().setTileset(tilemap, &tileset);

    TileData tile {tileId, 0};
    TilemapManager::get().setAt(tilemap, 0, 0, tile);
    TilemapManager::get().setAt(tilemap, 31, 31, tile);
    TilemapManager::get().setAt(tilemap, 32, 0, tile);
    TilemapManager::get().setAt(tilemap, -1, -1, tile);
    TilemapManager::get().setAt(tilemap, 5, 5, tile);

    GlShader quadShader("game/assets/shaders/QuadShader.glsl");
    GlShader tilemapShader("game/assets/shaders/TilemapShader.glsl");
    GlShader ppShader("game/assets/shaders/PostProcessingShader.glsl");
    Renderer::get().init(10000, &quadShader);
    TilemapRenderer::get().init(&tilemapShader);

    Input::get().addAxis("Horizontal", {KeyCode::D, KeyCode::A, KeyCode::Right, KeyCode::Left});
    Input::get().addAxis("Vertical", {KeyCode::W, KeyCode::S, KeyCode::Up, KeyCode::Down});
    Input::get().addAxis("Zoom", {KeyCode::E, KeyCode::Q});

    bool loggedCount = false;
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

            Renderer::get().setRenderWindowId(id);
            Renderer::get().setViewProjMat(registry);

            Renderer::get().beginPass();
            Renderer::get().clearColor(Color(0.1f, 0.1f, 0.15f, 1.0f));
            TilemapRenderer::get().render(tilemap, Renderer::get().getViewProjMat());

            if (!loggedCount) {
                LOG_INFO(
                    "      total indices = {} (expected 5 tiles -> 30)",
                    TilemapRenderer::get().totalIndexCount(tilemap)
                );
                loggedCount = true;
            }

            Renderer::get().beginPass();
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
