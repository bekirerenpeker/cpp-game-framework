#include "EngineInclude.hpp"
#include "test_funcs.hpp"
#include <cmath>
#include <glad/glad.h>

using namespace Engine;

// Interactive rule-tile paint test. A mouse-following indicator snaps to the
// tile grid (accounting for camera pan/zoom); LMB paints a rule tile, RMB
// erases. TilemapRenderer bakes each rule tile's region + rotation from its
// live 8-neighbor bitmask at build time. WASD/QE pan and zoom; V toggles
// wireframe.
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
    int tileCount =
        static_cast<int>(tileset.getAtlas().fromCellSize("tile", 16, 16, {0, 48, 64, 64}).size());

    if (tileCount < 1) {
        LOG_ERROR("      ERROR: tileset produced too few tiles (is TilesetFloorB.png present?)");
        return 1;
    }

    // Sixteen needs only 16 atlas cells; swapping to RuleTileTemplate::Full256
    // is a one-line change. The bitmask->tile tables are blank for now, so every
    // placement shows region 0 with no rotation until they're filled in by hand.
    uint16_t ruleTileId =
        tileset.createRuleTile("ruleTile", "tile", 0, tileCount, RuleTileTemplate::Sixteen);

    TilemapComponent tilemap;
    TilemapManager::get().setTileset(tilemap, &tileset);

    GlShader tilemapShader("game/assets/shaders/TilemapShader.glsl");
    GlShader quadShader("game/assets/shaders/QuadShader.glsl");
    GlTexture whiteTexture(COLOR_WHITE);
    Renderer::get().init(10000, &quadShader);
    TilemapRenderer::get().init(&tilemapShader);

    Input::get().addAxis("Horizontal", {KeyCode::D, KeyCode::A, KeyCode::Right, KeyCode::Left});
    Input::get().addAxis("Vertical", {KeyCode::W, KeyCode::S, KeyCode::Up, KeyCode::Down});
    Input::get().addAxis("Zoom", {KeyCode::E, KeyCode::Q});

    bool wireframe = false;
    std::vector<IdType> windowsToClose;
    while (WindowManager::get().anyWindowOpen()) {
        windowsToClose.clear();
        Time::get().update();
        float dt = Time::get().deltaTime();

        for (auto& [id, window] : WindowManager::get().getAllWindows()) {
            Input::get().update(id);

            Vec2 mouseWorld = VEC2_ZERO;
            View<TransformComponent, CameraComponent> camView(registry);
            for (const auto& [ent, trans, cam] : camView) {
                if (cam.windowId != id) continue;
                trans.position.x += Input::get().getAxis("Horizontal") * dt * cam.orthoSize;
                trans.position.y += Input::get().getAxis("Vertical") * dt * cam.orthoSize;
                cam.orthoSize -= Input::get().getAxis("Zoom") * dt * cam.orthoSize;

                // Mouse -> world, no rotation: getMousePos() is already window-centered,
                // Y-up pixels, so it maps linearly onto the camera's ortho half-extents.
                float halfW = cam.orthoSize * window->getAspectRatio() * 0.5f;
                float halfH = cam.orthoSize * 0.5f;
                Vec2 mousePx = Input::get().getMousePos();
                Vec2 ndc = Vec2(
                    mousePx.x / (window->getWidth() * 0.5f),
                    mousePx.y / (window->getHeight() * 0.5f)
                );
                mouseWorld =
                    Vec2(trans.position.x, trans.position.y) + Vec2(ndc.x * halfW, ndc.y * halfH);
                break;
            }

            int tileX = static_cast<int>(std::floor(mouseWorld.x));
            int tileY = static_cast<int>(std::floor(mouseWorld.y));

            if (Input::get().mouseButtonHeld(MouseButton::Left)) {
                TilemapManager::get().setAt(tilemap, tileX, tileY, {ruleTileId, 0});
            }
            if (Input::get().mouseButtonHeld(MouseButton::Right)) {
                TilemapManager::get().setAt(tilemap, tileX, tileY, {0, 0});
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

            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);   // keep the indicator quad solid
            Renderer::get().setShader(&quadShader);
            Renderer::get().addQuad(
                Vec2(tileX + 0.5f, tileY + 0.5f), Vec2(1, 1), Color(1, 1, 1, 0.35f), &whiteTexture
            );
            Renderer::get().endScene();

            window->swapBuffers();
        }

        GlfwContext::pollEvents();
        for (const auto& id : windowsToClose) WindowManager::get().closeWindow(id);
    }

    LOG_INFO("==========================================================\n");
    return 0;
}
