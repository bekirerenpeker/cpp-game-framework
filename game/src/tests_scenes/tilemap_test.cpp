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

    // Partitioning lives on the atlas. Slice the whole sheet into 16x16 regions
    // "tile0", "tile1", ... (transparent cells are skipped); the count is the
    // number of regions produced.
    Tileset tileset("game/assets/images/TilesetFloorB.png");
    int tileCount = static_cast<int>(tileset.getAtlas().fromCellSize("tile", 16, 16).size());

    if (tileCount < 3) {
        LOG_ERROR("      ERROR: tileset produced too few tiles (is TilesetFloorB.png present?)");
        return 1;
    }

    int mapWidth = 200;
    int mapHeight = 200;
    // Scales the noise sample coords; too low and the whole map crosses the
    // threshold at once, too high and the pattern turns to static.
    float frequency = 0.1f;

    // Three animated tiles sharing a frame count but differing in frame time,
    // each cycling a third of the atlas regions by prefix + index range.
    int f = tileCount / 3;
    uint16_t animTiles[] = {
        tileset.createAnimatedTile("animSlow", "tile", 0 * f, 1 * f, 0.5f),
        tileset.createAnimatedTile("animMed", "tile", 1 * f, 2 * f, 0.2f),
        tileset.createAnimatedTile("animFast", "tile", 2 * f, 3 * f, 0.07f),
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
