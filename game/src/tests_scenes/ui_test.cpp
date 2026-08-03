#include "EngineInclude.hpp"
#include "test_funcs.hpp"
#include <format>

using namespace Engine;

namespace {

const char* FONT_FOLDER = "game/assets/fonts";
const char* FALLBACK_FONT = "C:/Windows/Fonts/segoeui.ttf";
const char* IMAGE_PATH = "game/assets/images/mario.png";

const Vec2 CELL_SIZE(96.0f, 64.0f);
const Vec2 ROOT_MIN(560.0f, 380.0f);
const Vec2 WINDOW_MIN(230.0f, 150.0f);
const float SLIDER_HEIGHT = 20.0f;
const float GRIP_SIZE = 16.0f;
const float TITLE_BAR_HEIGHT = 28.0f;

// The caller owns every widget's value -- the UI stores nothing between frames, so a
// resizable window is just two vectors the scene keeps and hands back each frame.
float g_red = 0.30f, g_green = 0.62f, g_blue = 0.95f;
Vec2 g_rootSize(980.0f, 646.0f);
Vec2 g_windowPos(600.0f, 250.0f);
Vec2 g_windowSize(340.0f, 260.0f);

// Drag bookkeeping is caller-owned too. Only one node can hold capture at a time, so
// these can never be contended -- but each grip needs its own, since the size it
// started from is per widget. The *origin* is stored rather than accumulating
// per-frame deltas: hit rects are one frame stale, so deltas would compound into drift
// on a fast drag.
struct DragState
{
    bool active = false;
    Vec2 pointerOrigin = VEC2_ZERO;
    Vec2 valueOrigin = VEC2_ZERO;
};
DragState g_rootResize, g_windowResize, g_windowMove;

const Color BACKDROP(0.05f, 0.05f, 0.07f, 1.0f);
const Color SURFACE(0.13f, 0.14f, 0.18f);
const Color PANEL(0.17f, 0.18f, 0.23f);
const Color SWATCH(0.24f, 0.26f, 0.33f);
const Color ACCENT(0.30f, 0.62f, 0.95f);
const Color OUTLINE(0.28f, 0.31f, 0.40f);
const Color TITLE_COLOR(0.90f, 0.92f, 0.96f);
const Color LABEL_COLOR(0.82f, 0.85f, 0.91f);
const Color CAPTION_COLOR(0.56f, 0.59f, 0.68f);

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

UILayoutConfig box(UISizeSpec width, UISizeSpec height)
{
    UILayoutConfig config;
    config.width = width;
    config.height = height;
    return config;
}

UILayoutConfig textBox() { return box(UISizeSpec::grow(), UISizeSpec::fit()); }

void label(const Font& font, std::string_view text, Color color, float size)
{
    UIManager::get().addTextLeaf(
        textBox(), {
                       .font = &font, .text = text, .style = {.color = color, .size = size}
    }
    );
}

// A swatch of the style under test with its name underneath. The swatch's state comes
// back so a caller can rewrite the style it was just given -- nothing reads a style
// until draw(), so a hover landing here still lands in time.
UINodeState cell(const Font& font, const UIContainerStyle& style, const char* caption)
{
    UIManager& ui = UIManager::get();

    UILayoutConfig column = box(UISizeSpec::fixed(CELL_SIZE.x), UISizeSpec::fit());
    column.direction = UILayoutDirection::Column;
    column.gap = 6.0f;

    ui.openContainer(column);
    UINodeState state =
        ui.openContainer(box(UISizeSpec::grow(), UISizeSpec::fixed(CELL_SIZE.y)), style);
    ui.closeContainer();

    ui.addTextLeaf(
        textBox(), {
                       .font = &font,
                       .text = caption,
                       .style = {.color = CAPTION_COLOR, .size = 12.0f},
                       .alignment = {.horizontal = TextAlignH::Center}
    }
    );
    ui.closeContainer();

    return state;
}

// A grab handle pinned to its parent's bottom-right corner. Floating, so it costs the
// parent no layout space and can hang over whatever the parent's last child is.
UINodeState addGrip()
{
    UILayoutConfig layout = box(UISizeSpec::fixed(GRIP_SIZE), UISizeSpec::fixed(GRIP_SIZE));
    layout.isFloating = true;
    layout.floating.anchorX = UIAlign::End;
    layout.floating.anchorY = UIAlign::End;
    layout.floating.selfX = UIAlign::End;
    layout.floating.selfY = UIAlign::End;
    layout.floating.offset = Vec2(-4.0f, -4.0f);

    UINodeState grip =
        UIManager::get().openContainer(layout, {.backgroundColor = ACCENT, .borderRadius = 3.0f});
    UIManager::get().closeContainer();
    return grip;
}

// Drives a caller-owned vector from a drag. Reads isActive, not isHeld: the cursor
// leaves a 16px grip almost immediately, and only capture keeps the gesture alive.
// signY flips because the mouse is y-up while layout offsets are y-down, so dragging
// downwards has to *grow* a height and *increase* a top-left offset alike.
void dragVec2(const UINodeState& grip, DragState& drag, Vec2& value, Vec2 minValue, float signY)
{
    if (!grip.isActive) {
        drag.active = false;
        return;
    }

    Vec2 mouse = UIRenderer::get().getMouseUiPos();
    if (!drag.active) {
        drag.active = true;
        drag.pointerOrigin = mouse;
        drag.valueOrigin = value;
    }

    Vec2 delta = mouse - drag.pointerOrigin;
    value = Vec2(
        Math::max(drag.valueOrigin.x + delta.x, minValue.x),
        Math::max(drag.valueOrigin.y + delta.y * signY, minValue.y)
    );
}

// A movable, resizable window: a title bar that drags the whole thing, a body whose
// inner container re-wraps its text as the width changes, and a corner grip. Nothing
// here is engine state -- position and size are the scene's, exactly like the slider's
// float, which is what makes a window ordinary composition rather than a special case.
void resizableWindow(const Font& font)
{
    UIManager& ui = UIManager::get();

    UILayoutConfig windowLayout =
        box(UISizeSpec::fixed(g_windowSize.x), UISizeSpec::fixed(g_windowSize.y));
    windowLayout.isFloating = true;
    windowLayout.floating.offset = g_windowPos;
    windowLayout.direction = UILayoutDirection::Column;

    ui.openContainer(
        windowLayout, {
                          .backgroundColor = Color(0.16f, 0.17f, 0.22f),
                          .borderColor = OUTLINE,
                          .borderWidth = 1.0f,
                          .borderRadius = 10.0f,
                          .shadowColor = Color(0.0f, 0.0f, 0.0f, 0.75f),
                          .shadowOffset = Vec2(0.0f, 10.0f),
                          .shadowBlurRadius = 26.0f,
                          // Above the overlay's layer 1, so it is the topmost thing in
                          // the scene for both painting and hit testing.
                          .zIndex = 2,
                      }
    );

    UILayoutConfig titleLayout = box(UISizeSpec::grow(), UISizeSpec::fixed(TITLE_BAR_HEIGHT));
    titleLayout.padding = UIEdges(12.0f, 0.0f);
    titleLayout.alignCross = UIAlign::Center;

    UINodeState titleBar = ui.openContainer(
        titleLayout, {.backgroundColor = Color(0.24f, 0.26f, 0.34f), .borderRadius = 9.0f}
    );
    label(
        font, std::format("window  {} x {}", (int)g_windowSize.x, (int)g_windowSize.y), LABEL_COLOR,
        13.0f
    );
    ui.closeContainer();

    UILayoutConfig bodyLayout = box(UISizeSpec::grow(), UISizeSpec::grow());
    bodyLayout.padding = UIEdges(12.0f);
    ui.openContainer(bodyLayout);
    {
        UILayoutConfig innerLayout = box(UISizeSpec::grow(), UISizeSpec::grow());
        innerLayout.padding = UIEdges(10.0f);
        ui.openContainer(
            innerLayout, {
                             .backgroundColor = SWATCH,
                             .borderColor = OUTLINE,
                             .borderWidth = 1.0f,
                             .borderRadius = 6.0f,
                         }
        );
        ui.addTextLeaf(
            textBox(), {
                           .font = &font,
                           .text = "An inner container inside the window body. Its width is "
                                   "whatever the window leaves it, so this paragraph re-wraps "
                                   "while you drag the corner and the line count follows the "
                                   "box -- the layout is re-solved from scratch every frame.",
                           .style = {.color = Color(0.78f, 0.80f, 0.86f), .size = 13.0f},
        }
        );
        ui.closeContainer();
    }
    ui.closeContainer();

    UINodeState grip = addGrip();
    ui.closeContainer();

    // The title bar moves the window, so its y follows the pointer directly; the grip
    // grows it, so its y is inverted.
    dragVec2(titleBar, g_windowMove, g_windowPos, Vec2(-4000.0f), -1.0f);
    dragVec2(grip, g_windowResize, g_windowSize, WINDOW_MIN, -1.0f);
}

// A track with a handle floating along it, and the value lives in the caller's float.
// The drag reads isActive rather than isHeld: isHeld rides on hover and so would drop
// the grab the moment the cursor left the track, which is most of a real drag. The
// handle is a container too, so a press that starts on it captures *it* rather than
// the track -- either capture means the same gesture, and the value still comes off
// the track's own relativeMousePos, which stays meaningful outside its rect.
void slider(const Font& font, float& value, const char* caption)
{
    UIManager& ui = UIManager::get();

    UILayoutConfig column = box(UISizeSpec::fixed(CELL_SIZE.x * 1.6f), UISizeSpec::fit());
    column.direction = UILayoutDirection::Column;
    column.gap = 6.0f;
    ui.openContainer(column);

    UINodeState track = ui.openContainer(
        box(UISizeSpec::grow(), UISizeSpec::fixed(SLIDER_HEIGHT)),
        {.backgroundColor = SWATCH,
         .borderColor = OUTLINE,
         .borderWidth = 1.0f,
         .borderRadius = SLIDER_HEIGHT * 0.5f}
    );

    UILayoutConfig handleLayout =
        box(UISizeSpec::fixed(SLIDER_HEIGHT), UISizeSpec::fixed(SLIDER_HEIGHT));
    handleLayout.isFloating = true;
    handleLayout.floating.offset =
        Vec2(Math::clamp(value, 0.0f, 1.0f) * (CELL_SIZE.x * 1.6f - SLIDER_HEIGHT), 0.0f);

    UINodeState handle = ui.openContainer(
        handleLayout, {
                          .backgroundColor = ACCENT,
                          .borderColor = COLOR_WHITE,
                          .borderWidth = 1.0f,
                          .borderRadius = SLIDER_HEIGHT * 0.5f,
                      }
    );
    ui.closeContainer();
    ui.closeContainer();

    if (track.isActive || handle.isActive)
        value = Math::clamp(track.relativeMousePos.x, 0.0f, 1.0f);

    ui.addTextLeaf(
        textBox(), {
                       .font = &font,
                       .text = std::format("{}  {:.2f}", caption, value),
                       .style = {.color = CAPTION_COLOR, .size = 12.0f},
                       .alignment = {.horizontal = TextAlignH::Center}
    }
    );
    ui.closeContainer();
}

// A titled row of swatches; the caller puts cell()s between the two calls.
void beginSection(const Font& font, const char* title)
{
    UIManager& ui = UIManager::get();

    UILayoutConfig column = box(UISizeSpec::grow(), UISizeSpec::fit());
    column.direction = UILayoutDirection::Column;
    column.gap = 8.0f;

    ui.openContainer(column);
    label(font, title, LABEL_COLOR, 16.0f);

    UILayoutConfig row = box(UISizeSpec::grow(), UISizeSpec::fit());
    row.gap = 12.0f;
    ui.openContainer(row);
}

void endSection()
{
    UIManager::get().closeContainer();
    UIManager::get().closeContainer();
}

UINodeState beginPanel()
{
    UILayoutConfig layout = box(UISizeSpec::grow(), UISizeSpec::grow());
    layout.direction = UILayoutDirection::Column;
    layout.padding = UIEdges(16.0f);
    layout.gap = 16.0f;

    return UIManager::get().openContainer(
        layout, {
                    .backgroundColor = PANEL,
                    .borderColor = OUTLINE,
                    .borderWidth = 1.0f,
                    .borderRadius = 12.0f,
                }
    );
}

// Every reactive container in this scene lights the same way, so a screenshot shows
// at a glance which single one the hit test picked.
void lightWhenHovered(const UINodeState& state)
{
    if (!state.isHovered) return;

    UINode* node = UIManager::get().getNode(state.id);
    if (!node) return;
    node->style.backgroundColor = Color(0.20f, 0.34f, 0.52f);
    node->style.borderColor = ACCENT;
    node->style.borderWidth = 2.0f;
}

void buildBackgroundSection(const Font& font, GlTexture& image)
{
    beginSection(font, "Background");
    cell(font, {.backgroundColor = ACCENT}, "solid");
    cell(font, {.backgroundColor = Color(0.30f, 0.62f, 0.95f, 0.35f)}, "alpha 0.35");
    cell(font, {.backgroundImage = &image}, "image");
    cell(font, {.backgroundColor = Color(1.0f, 0.55f, 0.35f), .backgroundImage = &image}, "tinted");
    endSection();
}

void buildRadiusSection(const Font& font)
{
    beginSection(font, "Corner radius");
    cell(font, {.backgroundColor = SWATCH, .borderRadius = 0.0f}, "0");
    cell(font, {.backgroundColor = SWATCH, .borderRadius = 10.0f}, "10");
    cell(font, {.backgroundColor = SWATCH, .borderRadius = 26.0f}, "26");
    // Clamped to half the shorter side, so an absurd radius is a capsule, not a
    // distance field folded inside out.
    cell(font, {.backgroundColor = SWATCH, .borderRadius = 999.0f}, "clamped");
    endSection();
}

void buildBorderSection(const Font& font)
{
    beginSection(font, "Border width");
    cell(
        font,
        {.backgroundColor = SWATCH,
         .borderColor = ACCENT,
         .borderWidth = 2.0f,
         .borderRadius = 8.0f},
        "2"
    );
    cell(
        font,
        {.backgroundColor = SWATCH,
         .borderColor = ACCENT,
         .borderWidth = 6.0f,
         .borderRadius = 8.0f},
        "6"
    );
    cell(
        font,
        {.backgroundColor = SWATCH,
         .borderColor = ACCENT,
         .borderWidth = 14.0f,
         .borderRadius = 8.0f},
        "14"
    );
    // No background at all: the ring is the whole element, and the panel behind it
    // shows through the middle.
    cell(font, {.borderColor = ACCENT, .borderWidth = 3.0f, .borderRadius = 8.0f}, "no fill");
    endSection();
}

void buildBorderStyleSection(const Font& font)
{
    beginSection(font, "Border style");
    cell(
        font,
        {.backgroundColor = SWATCH,
         .borderColor = ACCENT,
         .borderWidth = 4.0f,
         .borderRadius = 8.0f,
         .borderStyle = UIBorderStyle::Solid},
        "solid"
    );
    cell(
        font,
        {.backgroundColor = SWATCH,
         .borderColor = ACCENT,
         .borderWidth = 4.0f,
         .borderRadius = 8.0f,
         .borderStyle = UIBorderStyle::Dashed},
        "dashed"
    );
    cell(
        font,
        {.backgroundColor = SWATCH,
         .borderColor = ACCENT,
         .borderWidth = 4.0f,
         .borderRadius = 8.0f,
         .borderStyle = UIBorderStyle::Dotted},
        "dotted"
    );
    // The dash walk is arc length around the rounded boundary, so the pattern keeps
    // its spacing through the corners instead of bunching up.
    cell(
        font,
        {.backgroundColor = SWATCH,
         .borderColor = ACCENT,
         .borderWidth = 4.0f,
         .borderRadius = 30.0f,
         .borderStyle = UIBorderStyle::Dashed},
        "dashed round"
    );
    endSection();
}

void buildShadowSection(const Font& font)
{
    beginSection(font, "Shadow");
    cell(
        font,
        {.backgroundColor = SWATCH,
         .borderRadius = 10.0f,
         .shadowColor = Color(0.0f, 0.0f, 0.0f, 0.8f),
         .shadowOffset = Vec2(6.0f, 8.0f),
         .shadowBlurRadius = 2.0f},
        "drop"
    );
    cell(
        font,
        {.backgroundColor = SWATCH,
         .borderRadius = 10.0f,
         .shadowColor = Color(0.0f, 0.0f, 0.0f, 0.85f),
         .shadowOffset = Vec2(0.0f, 6.0f),
         .shadowBlurRadius = 16.0f},
        "blurred"
    );
    // A zero offset makes the shadow a centred glow rather than a drop.
    cell(
        font,
        {.backgroundColor = SWATCH,
         .borderRadius = 10.0f,
         .shadowColor = ACCENT,
         .shadowBlurRadius = 18.0f},
        "glow"
    );
    // The shadow is knocked out under the box the way CSS does it, so this barely
    // tinted fill shows the panel behind it rather than its own shadow.
    cell(
        font,
        {.backgroundColor = Color(1.0f, 1.0f, 1.0f, 0.12f),
         .borderRadius = 10.0f,
         .shadowColor = Color(0.0f, 0.0f, 0.0f, 0.85f),
         .shadowBlurRadius = 14.0f},
        "knockout"
    );
    endSection();
}

void buildLiveSection(const Font& font)
{
    beginSection(font, "Per frame");
    cell(font, {.backgroundColor = SWATCH, .borderRadius = 2.0f + pulse(1.3f) * 30.0f}, "radius");
    cell(
        font,
        {.backgroundColor = SWATCH,
         .borderColor = ACCENT,
         .borderWidth = 1.0f + pulse(1.7f) * 13.0f,
         .borderRadius = 8.0f},
        "width"
    );
    cell(
        font,
        {.backgroundColor = SWATCH,
         .borderRadius = 10.0f,
         .shadowColor = ACCENT,
         .shadowBlurRadius = 2.0f + pulse(0.9f) * 26.0f},
        "blur"
    );

    UINodeState state = cell(font, {.backgroundColor = SWATCH, .borderRadius = 10.0f}, "hover me");
    if (state.isHovered) {
        UINode* node = UIManager::get().getNode(state.id);
        node->style.backgroundColor = ACCENT;
        node->style.borderColor = COLOR_WHITE;
        node->style.borderWidth = 3.0f;
        node->style.shadowColor = Color(ACCENT.r, ACCENT.g, ACCENT.b, 0.7f);
        node->style.shadowBlurRadius = 20.0f;
    }
    endSection();
}

// Declared from scratch every frame. Nesting is the open/close pairing, so no id is
// ever handled and the root is simply the container opened with nothing else open.
void buildUi(const Font& font, GlTexture& image)
{
    UIManager& ui = UIManager::get();
    ui.clear();

    UILayoutConfig rootLayout =
        box(UISizeSpec::fixed(g_rootSize.x), UISizeSpec::fixed(g_rootSize.y));
    rootLayout.direction = UILayoutDirection::Column;
    rootLayout.padding = UIEdges(24.0f);
    rootLayout.gap = 16.0f;

    ui.openContainer(
        rootLayout, {
                        .backgroundColor = SURFACE,
                        .borderColor = OUTLINE,
                        .borderWidth = 2.0f,
                        .borderRadius = 18.0f,
                        .shadowColor = Color(0.0f, 0.0f, 0.0f, 0.6f),
                        .shadowOffset = Vec2(0.0f, 12.0f),
                        .shadowBlurRadius = 28.0f,
                    }
    );
    {
        label(font, "Container styles", TITLE_COLOR, 26.0f);

        UILayoutConfig bodyLayout = box(UISizeSpec::grow(), UISizeSpec::grow());
        bodyLayout.gap = 16.0f;

        ui.openContainer(bodyLayout);
        {
            UINodeState leftPanel = beginPanel();
            buildBackgroundSection(font, image);
            buildRadiusSection(font);
            buildBorderSection(font);
            ui.closeContainer();
            lightWhenHovered(leftPanel);

            UINodeState rightPanel = beginPanel();
            buildBorderStyleSection(font);
            buildShadowSection(font);
            buildLiveSection(font);
            ui.closeContainer();
            lightWhenHovered(rightPanel);
        }
        ui.closeContainer();

        // Drag one past the end of its track: capture keeps the grab, and nothing
        // else lights up on the way past.
        beginSection(font, "Sliders - caller owns the value, capture owns the drag");
        slider(font, g_red, "red");
        slider(font, g_green, "green");
        slider(font, g_blue, "blue");
        ui.openContainer(
            box(UISizeSpec::fixed(CELL_SIZE.x), UISizeSpec::fixed(SLIDER_HEIGHT)),
            {.backgroundColor = Color(g_red, g_green, g_blue),
             .borderColor = OUTLINE,
             .borderWidth = 1.0f,
             .borderRadius = 6.0f}
        );
        ui.closeContainer();
        endSection();

        // The rest of UIContainerStyle is not the quad's business: overflow needs a
        // clip rect from the solver, and cursor and transition are input and
        // animation concerns.
        label(
            font,
            UIRenderer::get().getSpace() == UISpace::Screen ?
                "screen space - pinned to the window, camera does nothing.  SPACE switches" :
                "world space - pans and zooms with the camera.  SPACE switches",
            CAPTION_COLOR, 13.0f
        );

        // Floating, so it sits over the left panel instead of taking a slot in the
        // row, and declared last so it paints last -- which is exactly what makes it
        // the topmost hit. Hovering the overlap lights this and not the panel under
        // it; before the hit resolution both lit at once.
        //
        // zIndex is what puts it on its own paint layer. Without one it is still last
        // in the walk, but the walk only orders each batch internally -- the panel's
        // *glyphs* would flush after this box and land on top of it.
        UILayoutConfig overlayLayout = box(UISizeSpec::fixed(300.0f), UISizeSpec::fit());
        overlayLayout.isFloating = true;
        overlayLayout.floating.offset = Vec2(70.0f, 190.0f);
        overlayLayout.direction = UILayoutDirection::Column;
        overlayLayout.padding = UIEdges(14.0f);
        overlayLayout.gap = 6.0f;

        UINodeState overlay = ui.openContainer(
            overlayLayout, {
                               .backgroundColor = Color(0.22f, 0.24f, 0.32f),
                               .borderColor = OUTLINE,
                               .borderWidth = 1.0f,
                               .borderRadius = 10.0f,
                               .shadowColor = Color(0.0f, 0.0f, 0.0f, 0.7f),
                               .shadowOffset = Vec2(0.0f, 8.0f),
                               .shadowBlurRadius = 20.0f,
                               .zIndex = 1,
                           }
        );
        label(font, "floating overlay", LABEL_COLOR, 15.0f);
        label(font, "topmost wins: hover me and the panel below stays dark", CAPTION_COLOR, 11.0f);
        ui.closeContainer();
        lightWhenHovered(overlay);

        // Resizing the root re-solves everything under it: the panels redistribute,
        // the swatch rows keep their fixed cells and let the gaps absorb the change,
        // and the window's paragraph re-wraps. That is the layout test.
        UINodeState rootGrip = addGrip();
        dragVec2(rootGrip, g_rootResize, g_rootSize, ROOT_MIN, -1.0f);

        resizableWindow(font);
    }
    ui.closeContainer();
}

}   // namespace

// Every paintable field of UIContainerStyle, drawn by UIRenderer as one quad each --
// background, border ring, corner radius, dash pattern and shadow all come out of a
// single rounded-box distance field. SPACE switches the UI between screen and world
// space; WASD pans and QE zooms the camera, which moves the UI only in world space.
// Edges stay a pixel wide at any zoom because the antialiasing is driven by the
// distance field's gradient, and hovering proves hit testing follows the space.
int ui_test()
{
    IdType windowId = WindowManager::get().createWindow({1600, 800, "UI Test"});

    Registry registry;
    EntityHandle camera = registry.create();
    // A world-space root hangs down and right of the world origin, so the camera has
    // to look at the middle of that box rather than the first quadrant.
    camera.emplace<TransformComponent>().position =
        Vec3(g_rootSize.x * 0.5f, g_rootSize.y * -0.5f, 0);
    camera.emplace<CameraComponent>().windowId = windowId;
    camera.get<CameraComponent>().orthoSize = g_rootSize.y * 1.15f;

    Font font(findFontFile(), {.atlasType = FontAtlasType::Mtsdf, .emPixelSize = 48});
    GlTexture image(IMAGE_PATH, 0, GlTexture::FilterMode::Linear, GlTexture::WrapMode::ClampToEdge);

    GlShader quadShader("game/assets/shaders/QuadShader.glsl");
    GlShader textShader("game/assets/shaders/TextShader.glsl");
    GlShader uiShader("game/assets/shaders/UIBoxShader.glsl");
    Renderer::get().init(2000, &quadShader);
    TextRenderer::get().init(&textShader);
    UIRenderer::get().init(&uiShader);

    Input::get().addAxis("Horizontal", {KeyCode::D, KeyCode::A});
    Input::get().addAxis("Vertical", {KeyCode::W, KeyCode::S});
    Input::get().addAxis("Zoom", {KeyCode::E, KeyCode::Q});

    auto onWindowUpdate = [&](IdType id, float dt) {
        TransformComponent& transform = camera.get<TransformComponent>();
        CameraComponent& cam = camera.get<CameraComponent>();

        transform.position.x += Input::get().getAxis("Horizontal") * dt * cam.orthoSize;
        transform.position.y += Input::get().getAxis("Vertical") * dt * cam.orthoSize;
        cam.orthoSize -= Input::get().getAxis("Zoom") * dt * cam.orthoSize;

        if (Input::get().keyPressed(KeyCode::Space)) {
            UIRenderer::get().setSpace(
                UIRenderer::get().getSpace() == UISpace::Screen ? UISpace::World : UISpace::Screen
            );
        }

        buildUi(font, image);
    };

    auto onWindowRender = [&](IdType id, float dt) {
        Renderer::get().beginPass();
        Renderer::get().clearColor(BACKDROP);

        // Resolves its own batches in order; boxes and glyphs are not the caller's
        // flush to sequence.
        UIManager::get().draw();

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
