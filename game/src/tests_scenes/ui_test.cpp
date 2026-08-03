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
Vec2 g_windowSize(340.0f, 200.0f);

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

// Same face baked twice, differing only in atlas type. TAB swaps which one UIManager
// hands to every leaf that names no font; the pair at the bottom of the root names both
// so the two are comparable in a single frame.
const Font* g_mtsdfFont = nullptr;
const Font* g_bitmapFont = nullptr;

const Color BACKDROP(0.05f, 0.05f, 0.07f, 1.0f);
const Color SURFACE(0.13f, 0.14f, 0.18f);
const Color PANEL(0.17f, 0.18f, 0.23f);
const Color SWATCH(0.24f, 0.26f, 0.33f);
const Color ACCENT(0.30f, 0.62f, 0.95f);
const Color OUTLINE(0.28f, 0.31f, 0.40f);
const Color TITLE_COLOR(0.90f, 0.92f, 0.96f);
const Color LABEL_COLOR(0.82f, 0.85f, 0.91f);
const Color CAPTION_COLOR(0.56f, 0.59f, 0.68f);

// Every reactive container in this scene lights the same way, so a screenshot shows at
// a glance which single one the hit test picked. Only the fields it names are touched --
// the container keeps the rest of its own look.
const UIContainerStyle HOVER_LIGHT {
    .backgroundColor = Color(0.20f, 0.34f, 0.52f), .borderColor = ACCENT, .borderWidth = 2.0f
};

// The lit look of a swatch under the cursor, and the one a grab handle keeps for as long
// as it holds the drag. Named because a state style is an ordinary UIContainerStyle --
// nothing about it has to be written inline.
const UIContainerStyle HOVER_GLOW {
    .backgroundColor = ACCENT,
    .borderColor = COLOR_WHITE,
    .borderWidth = 3.0f,
    .shadowColor = Color(ACCENT.r, ACCENT.g, ACCENT.b, 0.7f),
    .shadowBlurRadius = 20.0f
};
const UIContainerStyle DRAG_GLOW {
    .backgroundColor = COLOR_WHITE, .shadowColor = ACCENT, .shadowBlurRadius = 12.0f
};

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

// No font anywhere in the scene's own signatures: a leaf with a null font takes
// UIManager's, and the only reason this one is passed through at all is the A/B pair at
// the bottom, which has to name a face to compare two of them in the same frame.
void label(std::string_view text, Color color, float size, const Font* font = nullptr)
{
    UIManager::get().addTextLeaf(
        textBox(), {
                       .font = font, .text = text, .style = {.color = color, .size = size}
    }
    );
}

// A swatch of the style under test with its name underneath. The swatch's state comes
// back so a caller can still rewrite the style it was just given -- nothing reads a
// style until draw(), so a hover landing here still lands in time.
UINodeState cell(const UIContainerStyleSpec& style, const char* caption)
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

    UINodeState grip = UIManager::get().openContainer(
        layout, {.backgroundColor = ACCENT,
                 .borderRadius = 3.0f,
                 .onHover = {.backgroundColor = COLOR_WHITE},
                 .onHeld = DRAG_GLOW}
    );
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
void resizableWindow()
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
                          // The vertical pass never shrinks a child below its content,
                          // so a short window is exactly the case where content spills
                          // past its own frame -- and this is what cuts it off. The
                          // shadow survives because a node's own drawing is bounded by
                          // its *parent's* clip, not the one it imposes on its children.
                          .overflow = UIOverflow::Hidden,
                          // Above the overlay's layer 1, so it is the topmost thing in
                          // the scene for both painting and hit testing.
                          .zIndex = 2,
                      }
    );

    UILayoutConfig titleLayout = box(UISizeSpec::grow(), UISizeSpec::fixed(TITLE_BAR_HEIGHT));
    titleLayout.padding = UIEdges(12.0f, 0.0f);
    titleLayout.alignCross = UIAlign::Center;

    UINodeState titleBar = ui.openContainer(
        titleLayout, {.backgroundColor = Color(0.24f, 0.26f, 0.34f),
                      .borderRadius = 9.0f,
                      .onHover = {.backgroundColor = Color(0.30f, 0.33f, 0.42f)},
                      .onHeld = {.backgroundColor = ACCENT}}
    );
    label(
        std::format("window  {} x {}", (int)g_windowSize.x, (int)g_windowSize.y), LABEL_COLOR, 13.0f
    );
    ui.closeContainer();

    UILayoutConfig bodyLayout = box(UISizeSpec::grow(), UISizeSpec::grow());
    bodyLayout.padding = UIEdges(12.0f);
    ui.openContainer(bodyLayout);
    {
        // clipY lets the box shrink below its content in the solve; overflow is what
        // stops the overflowing part from being *painted*. The two are separate on
        // purpose -- one is sizing, the other is rasterisation.
        UILayoutConfig innerLayout = box(UISizeSpec::grow(), UISizeSpec::grow());
        innerLayout.padding = UIEdges(10.0f);
        innerLayout.clipY = true;
        ui.openContainer(
            innerLayout, {
                             .backgroundColor = SWATCH,
                             .borderColor = OUTLINE,
                             .borderWidth = 1.0f,
                             .borderRadius = 6.0f,
                             .overflow = UIOverflow::Hidden,
                         }
        );
        ui.addTextLeaf(
            textBox(), {
                           .text = "An inner container inside the window body. Its width is "
                                   "whatever the window leaves it, so this paragraph re-wraps "
                                   "while you drag the corner and the line count follows the "
                                   "box -- the layout is re-solved from scratch every frame. "
                                   "This container also sets overflow to Hidden, so shrink the "
                                   "window and the text is cut off at the box edge instead of "
                                   "spilling over the border: the clip rect is the intersection "
                                   "of every clipping ancestor, handed to the shader per vertex, "
                                   "so it cuts mid-glyph rather than dropping whole lines.",
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
void slider(float& value, const char* caption)
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
         .borderRadius = SLIDER_HEIGHT * 0.5f,
         // Hover bubbles, so this also lights while the cursor is on the handle.
         .onHover = {.borderColor = ACCENT}}
    );

    UILayoutConfig handleLayout =
        box(UISizeSpec::fixed(SLIDER_HEIGHT), UISizeSpec::fixed(SLIDER_HEIGHT));
    handleLayout.isFloating = true;
    handleLayout.floating.offset =
        Vec2(Math::clamp(value, 0.0f, 1.0f) * (CELL_SIZE.x * 1.6f - SLIDER_HEIGHT), 0.0f);

    UINodeState handle = ui.openContainer(
        handleLayout,
        {
            .backgroundColor = ACCENT,
            .borderColor = COLOR_WHITE,
            .borderWidth = 1.0f,
            .borderRadius = SLIDER_HEIGHT * 0.5f,
            .onHover = {.borderWidth = 2.0f},
            // onHeld rides on capture, not hover, so the handle stays lit
            // for the whole drag -- the cursor is off a 20px handle almost
            // immediately, and a hover-bound style would flicker out there.
            .onHeld = {.borderWidth = 3.0f, .shadowColor = ACCENT, .shadowBlurRadius = 14.0f},
    }
    );
    ui.closeContainer();
    ui.closeContainer();

    if (track.isActive || handle.isActive)
        value = Math::clamp(track.relativeMousePos.x, 0.0f, 1.0f);

    // The state styles above are constants; this one *is* the value, which no constant
    // can express, so it stays on the escape hatch. A declared node is still writable
    // right up to draw().
    if (UINode* node = ui.getNode(handle.id)) {
        node->style.backgroundColor = Color(
            Math::lerp(ACCENT.r, 1.0f, value), Math::lerp(ACCENT.g, 1.0f, value),
            Math::lerp(ACCENT.b, 1.0f, value)
        );
    }

    ui.addTextLeaf(
        textBox(), {
                       .text = std::format("{}  {:.2f}", caption, value),
                       .style = {.color = CAPTION_COLOR, .size = 12.0f},
                       .alignment = {.horizontal = TextAlignH::Center}
    }
    );
    ui.closeContainer();
}

// A titled row of swatches; the caller puts cell()s between the two calls.
void beginSection(const char* title)
{
    UIManager& ui = UIManager::get();

    UILayoutConfig column = box(UISizeSpec::grow(), UISizeSpec::fit());
    column.direction = UILayoutDirection::Column;
    column.gap = 8.0f;

    ui.openContainer(column);
    label(title, LABEL_COLOR, 16.0f);

    UILayoutConfig row = box(UISizeSpec::grow(), UISizeSpec::fit());
    row.gap = 12.0f;
    ui.openContainer(row);
}

void endSection()
{
    UIManager::get().closeContainer();
    UIManager::get().closeContainer();
}

void beginPanel()
{
    UILayoutConfig layout = box(UISizeSpec::grow(), UISizeSpec::grow());
    layout.direction = UILayoutDirection::Column;
    layout.padding = UIEdges(16.0f);
    layout.gap = 16.0f;

    UIManager::get().openContainer(
        layout, {
                    .backgroundColor = PANEL,
                    .borderColor = OUTLINE,
                    .borderWidth = 1.0f,
                    .borderRadius = 12.0f,
                    .onHover = HOVER_LIGHT,
                }
    );
}

void buildBackgroundSection(GlTexture& image)
{
    beginSection("Background");
    cell({.backgroundColor = ACCENT}, "solid");
    cell({.backgroundColor = Color(0.30f, 0.62f, 0.95f, 0.35f)}, "alpha 0.35");
    cell({.backgroundImage = &image}, "image");
    cell({.backgroundColor = Color(1.0f, 0.55f, 0.35f), .backgroundImage = &image}, "tinted");
    endSection();
}

void buildRadiusSection()
{
    beginSection("Corner radius");
    cell({.backgroundColor = SWATCH, .borderRadius = 0.0f}, "0");
    cell({.backgroundColor = SWATCH, .borderRadius = 10.0f}, "10");
    cell({.backgroundColor = SWATCH, .borderRadius = 26.0f}, "26");
    // Clamped to half the shorter side, so an absurd radius is a capsule, not a
    // distance field folded inside out.
    cell({.backgroundColor = SWATCH, .borderRadius = 999.0f}, "clamped");
    endSection();
}

void buildBorderSection()
{
    beginSection("Border width");
    cell(
        {.backgroundColor = SWATCH,
         .borderColor = ACCENT,
         .borderWidth = 2.0f,
         .borderRadius = 8.0f},
        "2"
    );
    cell(
        {.backgroundColor = SWATCH,
         .borderColor = ACCENT,
         .borderWidth = 6.0f,
         .borderRadius = 8.0f},
        "6"
    );
    cell(
        {.backgroundColor = SWATCH,
         .borderColor = ACCENT,
         .borderWidth = 14.0f,
         .borderRadius = 8.0f},
        "14"
    );
    // No background at all: the ring is the whole element, and the panel behind it
    // shows through the middle.
    cell({.borderColor = ACCENT, .borderWidth = 3.0f, .borderRadius = 8.0f}, "no fill");
    endSection();
}

void buildBorderStyleSection()
{
    beginSection("Border style");
    cell(
        {.backgroundColor = SWATCH,
         .borderColor = ACCENT,
         .borderWidth = 4.0f,
         .borderRadius = 8.0f,
         .borderStyle = UIBorderStyle::Solid},
        "solid"
    );
    cell(
        {.backgroundColor = SWATCH,
         .borderColor = ACCENT,
         .borderWidth = 4.0f,
         .borderRadius = 8.0f,
         .borderStyle = UIBorderStyle::Dashed},
        "dashed"
    );
    cell(
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
        {.backgroundColor = SWATCH,
         .borderColor = ACCENT,
         .borderWidth = 4.0f,
         .borderRadius = 30.0f,
         .borderStyle = UIBorderStyle::Dashed},
        "dashed round"
    );
    endSection();
}

void buildShadowSection()
{
    beginSection("Shadow");
    cell(
        {.backgroundColor = SWATCH,
         .borderRadius = 10.0f,
         .shadowColor = Color(0.0f, 0.0f, 0.0f, 0.8f),
         .shadowOffset = Vec2(6.0f, 8.0f),
         .shadowBlurRadius = 2.0f},
        "drop"
    );
    cell(
        {.backgroundColor = SWATCH,
         .borderRadius = 10.0f,
         .shadowColor = Color(0.0f, 0.0f, 0.0f, 0.85f),
         .shadowOffset = Vec2(0.0f, 6.0f),
         .shadowBlurRadius = 16.0f},
        "blurred"
    );
    // A zero offset makes the shadow a centred glow rather than a drop.
    cell(
        {.backgroundColor = SWATCH,
         .borderRadius = 10.0f,
         .shadowColor = ACCENT,
         .shadowBlurRadius = 18.0f},
        "glow"
    );
    // The shadow is knocked out under the box the way CSS does it, so this barely
    // tinted fill shows the panel behind it rather than its own shadow.
    cell(
        {.backgroundColor = Color(1.0f, 1.0f, 1.0f, 0.12f),
         .borderRadius = 10.0f,
         .shadowColor = Color(0.0f, 0.0f, 0.0f, 0.85f),
         .shadowBlurRadius = 14.0f},
        "knockout"
    );
    endSection();
}

void buildLiveSection()
{
    beginSection("Per frame");
    cell({.backgroundColor = SWATCH, .borderRadius = 2.0f + pulse(1.3f) * 30.0f}, "radius");
    cell(
        {.backgroundColor = SWATCH,
         .borderColor = ACCENT,
         .borderWidth = 1.0f + pulse(1.7f) * 13.0f,
         .borderRadius = 8.0f},
        "width"
    );
    cell(
        {.backgroundColor = SWATCH,
         .borderRadius = 10.0f,
         .shadowColor = ACCENT,
         .shadowBlurRadius = 2.0f + pulse(0.9f) * 26.0f},
        "blur"
    );

    // The whole reactive look declared in one call. A press keeps the hover's glow and
    // only overrides the two fields it names, since each state is merged over the last.
    cell(
        {
            .backgroundColor = SWATCH,
            .borderRadius = 10.0f,
            .onHover = HOVER_GLOW,
            .onHeld = {.backgroundColor = Color(0.16f, 0.40f, 0.70f), .borderWidth = 1.0f},
            .onPressed = {.backgroundColor = COLOR_WHITE}
    },
        "press me"
    );
    endSection();
}

// Declared from scratch every frame. Nesting is the open/close pairing, so no id is
// ever handled and the root is simply the container opened with nothing else open.
void buildUi(GlTexture& image)
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
                        .overflow = UIOverflow::Hidden,
                    }
    );
    {
        label("Container styles", TITLE_COLOR, 26.0f);

        UILayoutConfig bodyLayout = box(UISizeSpec::grow(), UISizeSpec::grow());
        bodyLayout.gap = 16.0f;

        ui.openContainer(bodyLayout);
        {
            beginPanel();
            buildBackgroundSection(image);
            buildRadiusSection();
            buildBorderSection();
            ui.closeContainer();

            beginPanel();
            buildBorderStyleSection();
            buildShadowSection();
            buildLiveSection();
            ui.closeContainer();
        }
        ui.closeContainer();

        // Drag one past the end of its track: capture keeps the grab, and nothing
        // else lights up on the way past.
        beginSection("Sliders - caller owns the value, capture owns the drag");
        slider(g_red, "red");
        slider(g_green, "green");
        slider(g_blue, "blue");
        ui.openContainer(
            box(UISizeSpec::fixed(CELL_SIZE.x), UISizeSpec::fixed(SLIDER_HEIGHT)),
            {.backgroundColor = Color(g_red, g_green, g_blue),
             .borderColor = OUTLINE,
             .borderWidth = 1.0f,
             .borderRadius = 6.0f}
        );
        ui.closeContainer();
        endSection();

        // The two labels on the right are the same string from the same face at the same
        // size, one atlas each, so the readability difference is visible in one frame
        // instead of from memory across a TAB press. They cost no extra draw call: a
        // glyph carries its texture slot *and* its unitRange per vertex, so a second
        // atlas -- and a second atlas type -- still shares the batch.
        UILayoutConfig statusRow = box(UISizeSpec::grow(), UISizeSpec::fit());
        statusRow.gap = 18.0f;
        ui.openContainer(statusRow);
        label(
            UIRenderer::get().getSpace() == UISpace::Screen ?
                "screen space - pinned to the window.  SPACE switches" :
                "world space - pans and zooms with the camera.  SPACE switches",
            CAPTION_COLOR, 13.0f
        );
        label(
            std::format(
                "TAB atlas: {}", UIManager::get().getFont() == g_bitmapFont ? "bitmap" : "mtsdf"
            ),
            LABEL_COLOR, 13.0f
        );
        label("mtsdf Handgloves 138", CAPTION_COLOR, 11.0f, g_mtsdfFont);
        label("bitmap Handgloves 138", CAPTION_COLOR, 11.0f, g_bitmapFont);
        ui.closeContainer();

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

        ui.openContainer(
            overlayLayout, {
                               .backgroundColor = Color(0.22f, 0.24f, 0.32f),
                               .borderColor = OUTLINE,
                               .borderWidth = 1.0f,
                               .borderRadius = 10.0f,
                               .shadowColor = Color(0.0f, 0.0f, 0.0f, 0.7f),
                               .shadowOffset = Vec2(0.0f, 8.0f),
                               .shadowBlurRadius = 20.0f,
                               .zIndex = 1,
                               .onHover = HOVER_LIGHT,
                           }
        );
        label("floating overlay", LABEL_COLOR, 15.0f);
        label("topmost wins: hover me and the panel below stays dark", CAPTION_COLOR, 11.0f);
        ui.closeContainer();

        // Resizing the root re-solves everything under it: the panels redistribute,
        // the swatch rows keep their fixed cells and let the gaps absorb the change,
        // and the window's paragraph re-wraps. That is the layout test.
        UINodeState rootGrip = addGrip();
        dragVec2(rootGrip, g_rootResize, g_rootSize, ROOT_MIN, -1.0f);

        resizableWindow();
    }
    ui.closeContainer();
}

}   // namespace

// Every paintable field of UIContainerStyle, drawn by UIRenderer as one quad each --
// background, border ring, corner radius, dash pattern and shadow all come out of a
// single rounded-box distance field. SPACE switches the UI between screen and world
// space, TAB switches the UI font between an mtsdf and a bitmap atlas of the same face;
// WASD pans and QE zooms the camera, which moves the UI only in world space.
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

    // One face, two atlases, same em size, so TAB changes exactly one variable. The UI
    // is told about the font once here instead of every leaf carrying one -- drop the
    // setFont call and it bakes a system font rather than drawing nothing.
    fs::path fontFile = findFontFile();
    Font mtsdfFont(fontFile, {.atlasType = FontAtlasType::Mtsdf, .emPixelSize = 48});
    Font bitmapFont(fontFile, {.atlasType = FontAtlasType::Bitmap, .emPixelSize = 48});
    g_mtsdfFont = &mtsdfFont;
    g_bitmapFont = &bitmapFont;
    UIManager::get().setFont(g_mtsdfFont);

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

        if (Input::get().keyPressed(KeyCode::Tab)) {
            UIManager::get().setFont(
                UIManager::get().getFont() == g_mtsdfFont ? g_bitmapFont : g_mtsdfFont
            );
        }

        buildUi(image);
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
