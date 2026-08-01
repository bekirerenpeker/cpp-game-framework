#include "EngineInclude.hpp"
#include "test_funcs.hpp"
#include "utils/math/MathFuncs.hpp"

using namespace Engine;

namespace {

const char* FONT_FOLDER = "game/assets/fonts";
const char* FALLBACK_FONT = "C:/Windows/Fonts/segoeui.ttf";

const Vec2 ROOT_SIZE(900.0f, 600.0f);
const float ROOT_WIDTH_SWING = 150.0f;
const float GLOW_SPEED = 2.2f;
const float SPAN_SIZE_SPEED = 1.4f;

// 0..1, so a style field can be written as base + pulse * range.
float pulse(float speed) { return 0.5f + 0.5f * Math::sin(Time::get().currTime() * speed); }

fs::path findFontFile()
{
    if (FileManager::get().doesPathExist(FONT_FOLDER)) {
        for (const auto& entry : fs::directory_iterator(FONT_FOLDER)) {
            std::string ext = entry.path().extension().string();
            if (ext == ".ttf" || ext == ".otf") return entry.path();
        }
    }
    return FALLBACK_FONT;
}

UIContainerStyle fill(Color color)
{
    UIContainerStyle style;
    style.backgroundColor = color;
    return style;
}

UILayoutConfig box(UISizeSpec width, UISizeSpec height)
{
    UILayoutConfig config;
    config.width = width;
    config.height = height;
    return config;
}

UILayoutConfig textBox() { return box(UISizeSpec::grow(), UISizeSpec::fit()); }

// Declared from scratch every frame. Nesting is the open/close pairing, so no id is
// ever handled and the root is simply the container opened with nothing else open.
void buildUi(const Font& font, float rootWidth)
{
    UIManager& ui = UIManager::get();
    ui.clear();

    UILayoutConfig rootLayout = box(UISizeSpec::fixed(rootWidth), UISizeSpec::fixed(ROOT_SIZE.y));
    rootLayout.direction = UILayoutDirection::Column;
    rootLayout.padding = UIEdges(20.0f);
    rootLayout.gap = 16.0f;

    ui.openContainer(rootLayout, fill(Color(0.10f, 0.11f, 0.14f)));
    {
        UILayoutConfig headerLayout = box(UISizeSpec::grow(), UISizeSpec::fixed(64.0f));
        headerLayout.padding = UIEdges(16.0f, 12.0f);
        headerLayout.alignCross = UIAlign::Center;

        // One span per word, each leaning on a different distance-field effect. Every
        // measurement except size is em, so these hold their proportions under zoom.
        const TextStyle outlined {
            .color = COLOR_WHITE,
            .size = 28.0f,
            .outlineColor = Color(0.15f, 0.55f, 1.0f),
            .outlineWidth = 0.06f,
            .boldness = 0.025f,
        };
        // A zero shadowOffset makes the shadow a centred glow rather than a drop. The
        // width pulses, and its peak stays under Font::getMaxEffectEm() (0.1125 em for
        // this bake) once the softness is added on -- past that the glow floods the quad.
        const TextStyle glowing {
            .color = COLOR_WHITE,
            .size = 28.0f,
            .shadowColor = COLOR_CYAN,
            .shadowWidth = 0.01f + pulse(GLOW_SPEED) * 0.07f,
            .shadowSoftness = 0.03f,
        };
        const TextStyle slanted {
            .color = Color(1.0f, 0.85f, 0.35f),
            .size = 28.0f,
            .shadowColor = Color(0.0f, 0.0f, 0.0f, 0.85f),
            .shadowWidth = 0.03f,
            .shadowSoftness = 0.02f,
            .shadowOffset = Vec2(0.05f, -0.05f),
            .italicSkew = 0.22f,
            .underline = true,
        };

        ui.openContainer(headerLayout, fill(Color(0.20f, 0.23f, 0.30f)));
        ui.addTextLeaf(
            textBox(), {
                           .font = &font,
                           .text = "/sUI/s /sSystem/s /sTest/s",
                           .style = {.color = COLOR_WHITE, .size = 28.0f},
                           .spanStyles = {outlined, glowing, slanted}
        }
        );
        ui.closeContainer();

        UILayoutConfig bodyLayout = box(UISizeSpec::grow(), UISizeSpec::grow());
        bodyLayout.gap = 16.0f;

        ui.openContainer(bodyLayout);
        {
            // Fixed-width sidebar against a growing content pane: as the root width
            // swings, the sidebar holds its 220 and the content absorbs every unit.
            UILayoutConfig sidebarLayout = box(UISizeSpec::fixed(220.0f), UISizeSpec::grow());
            sidebarLayout.direction = UILayoutDirection::Column;
            sidebarLayout.padding = UIEdges(12.0f);
            sidebarLayout.gap = 8.0f;

            ui.openContainer(sidebarLayout, fill(Color(0.14f, 0.16f, 0.20f)));
            for (int i = 0; i < 3; i++) {
                ui.openContainer(
                    box(UISizeSpec::grow(), UISizeSpec::fixed(36.0f)),
                    fill(Color(0.24f, 0.27f, 0.34f))
                );
                ui.closeContainer();
            }
            ui.closeContainer();

            UILayoutConfig contentLayout = box(UISizeSpec::grow(), UISizeSpec::grow());
            contentLayout.direction = UILayoutDirection::Column;
            contentLayout.padding = UIEdges(16.0f);
            contentLayout.gap = 12.0f;

            ui.openContainer(contentLayout, fill(Color(0.14f, 0.16f, 0.20f)));
            ui.addTextLeaf(
                textBox(),
                {
                    .font = &font, .text = "Content", .style = {.color = COLOR_CYAN, .size = 24.0f}
            }
            );
            // A span opens mid-sentence and the wrap has to survive it: a word may
            // straddle a "/s" boundary, and the line box takes the tallest style on it.
            // Animating that span's size drives both rules live -- the line holding it
            // grows and shrinks, and the re-wrap moves which words share it.
            ui.addTextLeaf(
                textBox(),
                {
                    .font = &font,
                    .text = "This paragraph is long enough that it has to wrap inside the content "
                            "pane, so the /sline count changes/s as the root width swings -- which "
                            "is the text leaf's height following its solved width, re-solved every "
                            "frame.",
                    .style = {.color = Color(0.78f, 0.80f, 0.86f), .size = 18.0f},
                    .spanStyles = {
                              {.color = COLOR_YELLOW,
                         .size = 18.0f + pulse(SPAN_SIZE_SPEED) * 20.0f,
                         .boldness = 0.02f}
                    }
            }
            );
            ui.closeContainer();
        }
        ui.closeContainer();

        UILayoutConfig footerLayout = box(UISizeSpec::grow(), UISizeSpec::fixed(40.0f));
        footerLayout.padding = UIEdges(12.0f, 8.0f);
        footerLayout.alignCross = UIAlign::Center;

        ui.openContainer(footerLayout, fill(Color(0.20f, 0.23f, 0.30f)));
        ui.addTextLeaf(
            textBox(), {
                           .font = &font,
                           .text = "footer",
                           .style = {.color = Color(0.70f, 0.73f, 0.80f), .size = 16.0f}
        }
        );
        ui.closeContainer();
    }
    ui.closeContainer();
}

}   // namespace

// Immediate-mode UI: the whole tree is declared in the update phase every frame and
// solved again in the render phase, so an animated root width re-wraps the text
// under it live. WASD pans, QE zooms.
int ui_test()
{
    IdType windowId = WindowManager::get().createWindow({1000, 800, "UI Test"});

    Registry registry;
    EntityHandle camera = registry.create();
    camera.emplace<TransformComponent>().position = Vec3(ROOT_SIZE.x * 0.5f, ROOT_SIZE.y * 0.5f, 0);
    camera.emplace<CameraComponent>().windowId = windowId;
    camera.get<CameraComponent>().orthoSize = ROOT_SIZE.y * 1.15f;

    // A wider distance range than the default 4 is what buys room for the header's
    // outline and glow: effect width is capped by Font::getMaxEffectEm().
    Font font(
        findFontFile(),
        {.atlasType = FontAtlasType::Mtsdf, .emPixelSize = 48, .distanceRangePixels = 12}
    );

    GlShader quadShader("game/assets/shaders/QuadShader.glsl");
    GlShader textShader("game/assets/shaders/TextShader.glsl");
    Renderer::get().init(2000, &quadShader);
    TextRenderer::get().init(&textShader);

    Input::get().addAxis("Horizontal", {KeyCode::D, KeyCode::A});
    Input::get().addAxis("Vertical", {KeyCode::W, KeyCode::S});
    Input::get().addAxis("Zoom", {KeyCode::E, KeyCode::Q});

    auto onWindowUpdate = [&](IdType id, float dt) {
        TransformComponent& transform = camera.get<TransformComponent>();
        CameraComponent& cam = camera.get<CameraComponent>();

        transform.position.x += Input::get().getAxis("Horizontal") * dt * cam.orthoSize;
        transform.position.y += Input::get().getAxis("Vertical") * dt * cam.orthoSize;
        cam.orthoSize -= Input::get().getAxis("Zoom") * dt * cam.orthoSize;

        buildUi(font, ROOT_SIZE.x + Math::sin(Time::get().currTime()) * ROOT_WIDTH_SWING);
    };

    auto onWindowRender = [&](IdType id, float dt) {
        Renderer::get().beginPass();
        Renderer::get().setShader(&quadShader);
        Renderer::get().clearColor(Color(0.05f, 0.05f, 0.07f, 1.0f));

        UIManager::get().draw();

        // Boxes and glyphs are separate batches, so the quads have to resolve
        // before any glyph is submitted or the two draw in whichever order.
        Renderer::get().endScene();
        TextRenderer::get().flush();

        Renderer::get().beginPass();
        Renderer::get().setShader(&quadShader);
        Renderer::get().drawToWindow();
    };

    Application app(registry);
    app.onWindowUpdate().bind(&onWindowUpdate);
    app.onWindowRender().bind(&onWindowRender);
    app.run();

    return 0;
}
