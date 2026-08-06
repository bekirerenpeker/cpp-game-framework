#include "EngineInclude.hpp"
#include "test_funcs.hpp"

using namespace Engine;

namespace {

const Color BACKDROP(0.05f, 0.05f, 0.07f, 1.0f);

// The only thing here that cannot live inside the demo window, because it has to *be* a
// root: Grow on a root now measures against the window instead of falling back to
// max-content, so this fills the screen and its aligns centre the card in it. In world
// space there is no window to grow into and it collapses back to its content.
void fullScreenMenu()
{
    using namespace UIWidgets;
    const UIThemeColors& colors = UITheming::colors();
    const UIThemeMetrics& metrics = UITheming::metrics();

    openContainer(
        {.width = UISizeSpec::grow(),
         .height = UISizeSpec::grow(),
         .alignMain = UIAlign::Center,
         .alignCross = UIAlign::Center},
        {.backgroundColor = Color(0.0f, 0.0f, 0.0f, 0.6f)}, "screenRoot"
    );

    openContainer(
        {.padding = UIEdges(metrics.spacing.xl),
         .gap = metrics.spacing.md,
         .direction = UILayoutDirection::Column,
         .alignCross = UIAlign::Center},
        {.backgroundColor = colors.surfaceRaised,
         .borderColor = colors.accent,
         .borderWidth = metrics.borderWidth.thick,
         .borderRadius = metrics.radius.lg}
    );

    text("Full-screen root");
    text("A Grow root now fills the window, so this card stays centred as it resizes.");
    horizontalDivider();

    // Three equal columns and then one cell spanning all of them -- the buttons stretch
    // to their column because a grid stretches its cells by default.
    openGrid({.columns = 3, .gridLayout = {.width = UISizeSpec::fixed(360.0f)}});
    button("Easy");
    button("Normal");
    button("Hard");
    button("Back", {.buttonLayout = {.gridSpan = 3}});
    closeGrid();

    horizontalDivider();
    text("F1 closes this.");

    closeContainer();
    closeContainer();
}

}   // namespace

// A bare harness: everything on screen comes out of UIWidgets::demoWindow(), and nothing
// here loads or configures anything -- no font, no theme, no shader. The UI's font is
// left unset, which resolves to FontLoader's default. SPACE toggles screen/world UI
// space, F1 the full-screen root, and WASD/QE pan and zoom the camera, which only moves
// the UI in world space.
int ui_test()
{
    IdType windowId = WindowManager::get().createWindow({1600, 800, "UI Test"});

    Registry registry;
    EntityHandle camera = registry.create();
    // A world-space root hangs down and right of the world origin, so the camera looks
    // at the middle of that box rather than the first quadrant.
    camera.emplace<TransformComponent>().position = Vec3(490.0f, -323.0f, 0);
    camera.emplace<CameraComponent>().windowId = windowId;
    camera.get<CameraComponent>().orthoSize = 743.0f;

    Renderer::get().init();
    TextRenderer::get().init();
    UIRenderer::get().init();

    Input::get().addAxis("Horizontal", {KeyCode::D, KeyCode::A});
    Input::get().addAxis("Vertical", {KeyCode::W, KeyCode::S});
    Input::get().addAxis("Zoom", {KeyCode::E, KeyCode::Q});

    bool menuOpen = false;

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
        if (Input::get().keyPressed(KeyCode::F1)) menuOpen = !menuOpen;
    };

    auto onWindowRender = [&](IdType id, float dt) {
        Renderer::get().beginPass();
        Renderer::get().clearColor(BACKDROP);

        Renderer::get().beginPass();
        Renderer::get().setShader(Renderer::get().getDefaultShader());
        Renderer::get().drawToWindow();

        UIWidgets::clear();
        // Declared first so it paints under the demo window: roots are drawn in the
        // order they were declared, and hit testing breaks ties the same way.
        if (menuOpen) fullScreenMenu();
        UIWidgets::demoWindow();
        UIManager::get().draw();
    };

    Application app(registry);
    app.onWindowUpdate().bind(&onWindowUpdate);
    app.onWindowRender().bind(&onWindowRender);
    app.run();

    return 0;
}
