#include "EngineInclude.hpp"
#include "test_funcs.hpp"
#include <glad/glad.h>

using namespace Engine;

// Draws overlapping sprite stacks through Renderer::renderSprites(), with each
// stack's entities created highest-layer-first so draw order can only come out
// right if SpriteComponent::layer is honoured. The spinning sprites carry their
// own grayscale shader, so they also exercise the mid-batch shader switch.
// WASD/QE pan and zoom; F flips, H hides the middle layer, C churns a middle
// sprite, V toggles wireframe, G toggles the spinners' shader.
int batch_renderer_test()
{
    Logger::get().setUseAsync(true);

    IdType windowId1 = WindowManager::get().createWindow(
        {1000, 800, "Sprite Test Window 1", WindowFlags::Transparent}
    );
    IdType windowId2 = WindowManager::get().createWindow(
        {1000, 800, "Sprite Test Window 2", WindowFlags::Transparent}
    );

    Registry registry;
    EntityHandle camera1 = registry.create();
    camera1.emplace<TransformComponent>();
    camera1.emplace<CameraComponent>().windowId = windowId1;
    camera1.get<CameraComponent>().orthoSize = 40;
    EntityHandle camera2 = registry.create();
    camera2.emplace<TransformComponent>();
    camera2.emplace<CameraComponent>().windowId = windowId2;
    camera2.get<CameraComponent>().orthoSize = 40;

    GlTexture marioTex(FileManager::get().gameAsset("images/mario.png"));
    TextureAtlas tileAtlas(FileManager::get().gameAsset("images/TilesetFloorB.png"));
    std::vector<std::string> tileKeys = tileAtlas.fromCellSize("tile", 32, 32);
    if (tileKeys.empty()) {
        LOG_ERROR("atlas produced no regions (is TilesetFloorB.png present?)");
        return 1;
    }

    GlShader grayShader(FileManager::get().gameAsset("shaders/GrayscaleQuadShader.glsl"));
    GlShader ppShader(FileManager::get().gameAsset("shaders/PostProcessingShader.glsl"));
    Renderer::get().init(10000);
    GlShader* shader = Renderer::get().getDefaultShader();

    constexpr int STACK_COUNT = 5, LAYERS_PER_STACK = 5;
    std::vector<Entity> stackSprites;
    for (int s = 0; s < STACK_COUNT; s++) {
        for (int layer = LAYERS_PER_STACK - 1; layer >= 0; layer--) {
            EntityHandle sprite = registry.create();

            int stackIndex = s - STACK_COUNT / 2;
            float stackX = stackIndex * 9.0f;
            TransformComponent& transform = sprite.emplace<TransformComponent>();
            transform.position = Vec3(stackX + layer * 1.2f, layer * 1.2f, 0);
            transform.scale = VEC2_ONE * 6.0f;

            SpriteComponent& quad = sprite.emplace<SpriteComponent>();
            quad.layer = layer;
            if (layer % 2 == 0) {
                quad.texture = &marioTex;
            } else {
                const TextureAtlas::Region* region =
                    tileAtlas.getRegion(tileKeys[layer % tileKeys.size()]);
                quad.texture = &tileAtlas.getTexture();
                quad.uvMin = region->uvMin;
                quad.uvMax = region->uvMax;
            }
            float shade = 0.35f + 0.65f * (layer / float(LAYERS_PER_STACK - 1));
            quad.color = Color(shade, shade, shade, 1);

            stackSprites.push_back(sprite.getEntity());
        }
    }

    std::vector<Entity> spinners;
    for (int s = 0; s < STACK_COUNT; s++) {
        EntityHandle spinner = registry.create();
        int stackIndex = s - STACK_COUNT / 2;
        float stackX = stackIndex * 9.0f;
        TransformComponent& transform = spinner.emplace<TransformComponent>();
        transform.position = Vec3(stackX + 2.4f, -7.0f, 0);
        transform.scale = VEC2_ONE * 4.0f;

        SpriteComponent& quad = spinner.emplace<SpriteComponent>();
        quad.texture = &marioTex;
        quad.layer = LAYERS_PER_STACK;
        quad.color = COLOR_YELLOW;
        quad.shader = &grayShader;
        spinners.push_back(spinner.getEntity());
    }

    Input::get().addAxis("Horizontal", {KeyCode::D, KeyCode::A, KeyCode::Right, KeyCode::Left});
    Input::get().addAxis("Vertical", {KeyCode::W, KeyCode::S, KeyCode::Up, KeyCode::Down});
    Input::get().addAxis("Zoom", {KeyCode::E, KeyCode::Q});

    bool flipped = false, hideMiddle = false, churn = false, wireframe = false, grayed = true;
    int churnLayer = LAYERS_PER_STACK / 2;

    auto onFrame = [&](float dt) {
        float time = Time::get().getCurrTime();

        if (churn) {
            EntityHandle victim(stackSprites[churnLayer], registry);
            TransformComponent transform = victim.get<TransformComponent>();
            SpriteComponent quad = victim.get<SpriteComponent>();
            victim.destroy();

            EntityHandle respawned = registry.create();
            respawned.emplace<TransformComponent>(transform);
            respawned.emplace<SpriteComponent>(quad);
            stackSprites[churnLayer] = respawned.getEntity();
        }

        for (Entity spinner : spinners) {
            EntityHandle(spinner, registry).get<TransformComponent>().rotation = time;
        }
    };

    auto onWindowUpdate = [&](IdType windowId, float dt) {
        View<TransformComponent, CameraComponent> camView(registry);
        for (const auto& [ent, trans, cam] : camView) {
            if (cam.windowId != windowId) continue;
            trans.position.x += Input::get().getAxis("Horizontal") * dt * cam.orthoSize;
            trans.position.y += Input::get().getAxis("Vertical") * dt * cam.orthoSize;
            cam.orthoSize -= Input::get().getAxis("Zoom") * dt * cam.orthoSize;
        }

        if (Input::get().keyPressed(KeyCode::F)) {
            flipped = !flipped;
            View<SpriteComponent> spriteView(registry);
            for (const auto& [ent, quad] : spriteView) quad.flipX = flipped;
        }
        if (Input::get().keyPressed(KeyCode::H)) {
            hideMiddle = !hideMiddle;
            View<SpriteComponent> spriteView(registry);
            for (const auto& [ent, quad] : spriteView) {
                if (quad.layer == churnLayer) quad.visible = !hideMiddle;
            }
        }
        if (Input::get().keyPressed(KeyCode::C)) churn = !churn;
        if (Input::get().keyPressed(KeyCode::V)) wireframe = !wireframe;
        if (Input::get().keyPressed(KeyCode::G)) {
            grayed = !grayed;
            for (Entity spinner : spinners) {
                EntityHandle(spinner, registry).get<SpriteComponent>().shader =
                    grayed ? &grayShader : nullptr;
            }
        }
    };

    auto onWindowRender = [&](IdType windowId, float dt) {
        Renderer::get().beginPass();
        Renderer::get().setShader(shader);
        Renderer::get().clearColor(Color(0.5f, 0.5f, 1.0f, 1.0f));

        Renderer::get().renderSprites(registry);

        glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
        Renderer::get().endScene();
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        Renderer::get().beginPass();
        Renderer::get().setShader(&ppShader);
        Renderer::get().drawToWindow();
    };

    Application app(registry);
    app.onFrame().bind(&onFrame);
    app.onWindowUpdate().bind(&onWindowUpdate);
    app.onWindowRender().bind(&onWindowRender);
    app.run();

    return 0;
}
