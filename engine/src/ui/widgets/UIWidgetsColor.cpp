#include "ui/widgets/UIWidgets.hpp"
#include "ui/widgets/UIWidgetsInternal.hpp"
#include "core/file_management/FileManager.hpp"
#include "core/input/Input.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "graphics/gl_wrappers/GlShader.hpp"
#include "ui/UIStateStore.hpp"
#include "ui/UiManager.hpp"
#include "utils/math/MathFuncs.hpp"
#include <format>
#include <string>

namespace Engine {

namespace UIWidgets {

namespace {

// The three markers sit on top of the hue ramp and the saturation/value square, so what
// they have to stay legible against is arbitrary colour rather than the theme's surface.
// Reading a theme role here made them vanish on the hue that happened to match it, which
// is why these are fixed and not UIThemeColors/UIThemeMetrics lookups. White on a dark
// outline reads on every hue. Sizes stay in design units, so the UI scale still applies
// to them the way it does to any other style length.
const Color MARKER_FILL = COLOR_WHITE;
const Color MARKER_OUTLINE(0.0f, 0.0f, 0.0f, 0.7f);
constexpr float MARKER_OUTLINE_THIN = 1.0f;
constexpr float MARKER_OUTLINE_THICK = 2.0f;

// Deliberately leaked: a static GlShader would run its GL deletes at teardown, after the
// context is already gone.
GlShader* colorPickerShader()
{
    static GlShader* shader = nullptr;
    static bool loaded = false;
    if (loaded) return shader;

    loaded = true;
    fs::path path = FileManager::get().engineAsset("shaders/UIColorPickerShader.glsl");
    if (!FileManager::get().doesPathExist(path)) {
        LOG_ERROR("colour picker shader missing at {}", path);
        return shader;
    }

    shader = new GlShader(path);
    return shader;
}

}   // namespace

UIInputState colorPicker(Color& color, const ColorPickerConfig& config)
{
    const UIThemeColors& colors = UITheming::colors();
    const UIThemeMetrics& metrics = UITheming::metrics();

    ColorPickerConfig defaultConfig = {
        .pickerLayout =
            {
                           .width = UISizeSpec::grow(),
                           .gap = metrics.spacing.sm,
                           .direction = UILayoutDirection::Column,
                           },
        .pickerStyle = {.blockInput = true, .cursor = UICursor::Crosshair},
        .squareLayout =
            {
                           .width = UISizeSpec::grow(),
                           .height = UISizeSpec::grow(),
                           },
        .hueLayout =
            {
                           .width = UISizeSpec::fixed(config.hueWidth > 0.0f ? config.hueWidth : 18.0f),
                           .height = UISizeSpec::grow(),
                           },
        .markerLayout =
            {
                           .width = UISizeSpec::fixed(12.0f),
                           .height = UISizeSpec::fixed(12.0f),
                           .floating =
                    UIFloatingConfig {
                        .anchorX = UIAlign::End,
                        .anchorY = UIAlign::End,
                        .selfX = UIAlign::Center,
                        .selfY = UIAlign::Center
                    }, .isFloating = true,
                           },
        .markerStyle =
            {
                           .borderColor = MARKER_FILL,
                           .borderWidth = MARKER_OUTLINE_THICK,
                           .borderRadius = 6.0f,
                           .shadowColor = MARKER_OUTLINE,
                           .shadowBlurRadius = 3.0f,
                           },
        .hueMarkerLayout =
            {
                           .width = UISizeSpec::percent(1.0f),
                           .height = UISizeSpec::fixed(4.0f),
                           .floating = UIFloatingConfig {.anchorY = UIAlign::End, .selfY = UIAlign::Center},
                           .isFloating = true,
                           },
        .hueMarkerStyle =
            {
                           .backgroundColor = MARKER_FILL,
                           .borderColor = MARKER_OUTLINE,
                           .borderWidth = MARKER_OUTLINE_THIN,
                           .borderRadius = 2.0f,
                           },
        .alphaLayout =
            {
                           .width = UISizeSpec::grow(),
                           .height = UISizeSpec::fixed(config.alphaHeight > 0.0f ? config.alphaHeight : 14.0f),
                           },
        .alphaMarkerLayout =
            {
                           .width = UISizeSpec::fixed(4.0f),
                           .height = UISizeSpec::percent(1.0f),
                           .floating = UIFloatingConfig {.anchorX = UIAlign::End, .selfX = UIAlign::Center},
                           .isFloating = true,
                           },
        .alphaMarkerStyle =
            {
                           .backgroundColor = MARKER_FILL,
                           .borderColor = MARKER_OUTLINE,
                           .borderWidth = MARKER_OUTLINE_THIN,
                           .borderRadius = 2.0f,
                           },
        .previewLayout =
            {
                           .width = UISizeSpec::grow(),
                           .height = UISizeSpec::fixed(22.0f),
                           },
        .previewStyle =
            {
                           .borderColor = colors.borderStrong,
                           .borderWidth = metrics.borderWidth.thin,
                           .borderRadius = metrics.radius.sm,
                           },
        .valuesLayout =
            {
                           .width = UISizeSpec::grow(),
                           .gap = 2.0f,
                           .direction = UILayoutDirection::Column,
                           },
        .valuesTextConfig = {.style = UITheming::textStyle("caption")},
    };

    defaultConfig.pickerLayout.combine(config.pickerLayout);
    defaultConfig.pickerStyle.combine(config.pickerStyle);
    defaultConfig.squareLayout.combine(config.squareLayout);
    defaultConfig.hueLayout.combine(config.hueLayout);
    defaultConfig.markerLayout.combine(config.markerLayout);
    defaultConfig.markerStyle.combine(config.markerStyle);
    defaultConfig.hueMarkerLayout.combine(config.hueMarkerLayout);
    defaultConfig.hueMarkerStyle.combine(config.hueMarkerStyle);
    defaultConfig.alphaLayout.combine(config.alphaLayout);
    defaultConfig.alphaMarkerLayout.combine(config.alphaMarkerLayout);
    defaultConfig.alphaMarkerStyle.combine(config.alphaMarkerStyle);
    defaultConfig.previewLayout.combine(config.previewLayout);
    defaultConfig.previewStyle.combine(config.previewStyle);
    defaultConfig.valuesLayout.combine(config.valuesLayout);
    defaultConfig.valuesTextConfig.style.combine(config.valuesTextConfig.style);

    float squareHeight = config.squareHeight > 0.0f ? config.squareHeight : 120.0f;

    UINodeState picker =
        openContainer(defaultConfig.pickerLayout, defaultConfig.pickerStyle, config.key);

    // Hue and saturation have no meaning in a black or greyscale RGB value, so they are
    // kept per picker instead of being re-derived from the colour every frame and lost.
    UIStateStore& store = UIStateStore::get();
    Vec3 fromColor = color.toHsv();
    float& storedHue = store.value(picker.persistentKey, "pickerHue", fromColor.x);
    float& storedSaturation = store.value(picker.persistentKey, "pickerSaturation", fromColor.y);
    float& storedValue = store.value(picker.persistentKey, "pickerValue", fromColor.z);

    // The caller may have written the colour itself since last frame; only then is the
    // stored hue thrown away, so dragging to black does not lose it.
    Vec3 hsv(storedHue, storedSaturation, storedValue);
    Color fromState = Color::fromHsv(hsv, color.a);
    if (Math::abs(fromState.r - color.r) > 0.001f || Math::abs(fromState.g - color.g) > 0.001f ||
        Math::abs(fromState.b - color.b) > 0.001f) {
        hsv = fromColor;
    }

    openContainer({
        .width = UISizeSpec::grow(),
        .height = UISizeSpec::fixed(squareHeight),
        .gap = metrics.spacing.sm,
    });

    // Neither marker can be placed in pixels, since the square's size is only known once
    // the solve runs. A floating Percent-sized spacer reaches exactly the fraction, and
    // the marker anchors to its far corner -- the same trick the slider handle uses.
    // A zero intrinsic size, because these fill whatever their container is: the default
    // reports itself as min-content too, and nothing can be shrunk below that -- the leaf
    // would keep its own width and paint straight out of a narrower parent.
    UINodeState square = openContainer(defaultConfig.squareLayout);
    addShaderLeaf(
        {.width = UISizeSpec::grow(), .height = UISizeSpec::grow()},
        {.shader = colorPickerShader(),
         .params0 = Vec4(0.0f, hsv.x, 0.0f, 1.0f),
         .intrinsicSize = VEC2_ZERO}
    );
    UINodeState squareAnchor = openContainer({
        .width = UISizeSpec::percent(Math::clamp(hsv.y, 0.0f, 1.0f)),
        .height = UISizeSpec::percent(Math::clamp(1.0f - hsv.z, 0.0f, 1.0f)),
        .isFloating = true,
    });
    UINodeState squareMarker = openContainer(defaultConfig.markerLayout, defaultConfig.markerStyle);
    closeContainer();
    closeContainer();
    closeContainer();

    UINodeState hueStrip = openContainer(defaultConfig.hueLayout);
    addShaderLeaf(
        {.width = UISizeSpec::grow(), .height = UISizeSpec::grow()},
        {.shader = colorPickerShader(),
         .params0 = Vec4(1.0f, 0.0f, 0.0f, 1.0f),
         .intrinsicSize = VEC2_ZERO}
    );
    UINodeState hueAnchor = openContainer({
        .width = UISizeSpec::percent(1.0f),
        .height = UISizeSpec::percent(Math::clamp(hsv.x, 0.0f, 1.0f)),
        .isFloating = true,
    });
    UINodeState hueMarker =
        openContainer(defaultConfig.hueMarkerLayout, defaultConfig.hueMarkerStyle);
    closeContainer();
    closeContainer();
    closeContainer();

    closeContainer();

    float alpha = Math::clamp(color.a, 0.0f, 1.0f);
    Color opaque = Color::fromHsv(hsv, 1.0f);

    UINodeState alphaStrip = openContainer(defaultConfig.alphaLayout);
    addShaderLeaf(
        {.width = UISizeSpec::grow(), .height = UISizeSpec::grow()},
        {.shader = colorPickerShader(),
         .params0 = Vec4(2.0f, 0.0f, 0.0f, 1.0f),
         .params1 = Vec4(opaque.r, opaque.g, opaque.b, 1.0f),
         .intrinsicSize = VEC2_ZERO}
    );
    UINodeState alphaAnchor = openContainer({
        .width = UISizeSpec::percent(alpha),
        .height = UISizeSpec::percent(1.0f),
        .isFloating = true,
    });
    UINodeState alphaMarker =
        openContainer(defaultConfig.alphaMarkerLayout, defaultConfig.alphaMarkerStyle);
    closeContainer();
    closeContainer();
    closeContainer();

    // The markers and their spacers sit over the shader quads, so a press can land on
    // any of them; all of them drive the value off their own control's rect.
    Vec3 heldHsv = hsv;
    float heldAlpha = alpha;

    bool isEditing = square.isActive || squareAnchor.isActive || squareMarker.isActive ||
                     hueStrip.isActive || hueAnchor.isActive || hueMarker.isActive ||
                     alphaStrip.isActive || alphaAnchor.isActive || alphaMarker.isActive;

    if (square.isActive || squareAnchor.isActive || squareMarker.isActive) {
        hsv.y = Math::clamp(square.relativeMousePos.x, 0.0f, 1.0f);
        hsv.z = 1.0f - Math::clamp(square.relativeMousePos.y, 0.0f, 1.0f);
    }
    if (hueStrip.isActive || hueAnchor.isActive || hueMarker.isActive)
        hsv.x = Math::clamp(hueStrip.relativeMousePos.y, 0.0f, 0.9999f);
    if (alphaStrip.isActive || alphaAnchor.isActive || alphaMarker.isActive)
        alpha = Math::clamp(alphaStrip.relativeMousePos.x, 0.0f, 1.0f);

    // Measured on hsv rather than on the colour, which is rebuilt from it every frame
    // and so drifts by a bit or two even when nothing was touched.
    bool isChanged =
        hsv.x != heldHsv.x || hsv.y != heldHsv.y || hsv.z != heldHsv.z || alpha != heldAlpha;

    color = Color::fromHsv(hsv, alpha);

    storedHue = hsv.x;
    storedSaturation = hsv.y;
    storedValue = hsv.z;

    UIContainerStyleSpec previewStyle = defaultConfig.previewStyle;
    previewStyle.backgroundColor = color;
    openContainer(defaultConfig.previewLayout, previewStyle);
    closeContainer();

    if (config.showValues) {
        openContainer(defaultConfig.valuesLayout);

        UITextConfig valueText = config.valuesTextConfig;
        valueText.style = defaultConfig.valuesTextConfig.style;

        std::string rgba =
            std::format("RGBA  {:.2f}  {:.2f}  {:.2f}  {:.2f}", color.r, color.g, color.b, color.a);
        valueText.text = rgba;
        addTextLeaf({}, valueText);

        std::string hsva =
            std::format("HSVA  {:.2f}  {:.2f}  {:.2f}  {:.2f}", hsv.x, hsv.y, hsv.z, color.a);
        valueText.text = hsva;
        addTextLeaf({}, valueText);

        closeContainer();
    }

    closeContainer();
    return dragInputState(picker, isEditing, isChanged);
}

UIInputState colorPickerPopup(Color& color, const ColorPickerPopupConfig& config)
{
    const UIThemeColors& colors = UITheming::colors();
    const UIThemeMetrics& metrics = UITheming::metrics();

    ColorPickerPopupConfig defaultConfig = {
        .wrapperLayout = {},
        .swatchLayout =
            {
                          .width = UISizeSpec::fixed(46.0f),
                          .height = UISizeSpec::fixed(metrics.controlHeightSmall + 2.0f),
                          },
        .swatchStyle =
            {
                          .borderColor = colors.borderStrong,
                          .borderWidth = metrics.borderWidth.thin,
                          .borderRadius = metrics.radius.sm,
                          .blockInput = true,
                          .cursor = UICursor::Pointer,
                          .onHover = {.borderColor = colors.borderFocus},
                          },
        .panelLayout =
            {
                          .width = UISizeSpec::fixed(220.0f),
                          .padding = UIEdges(metrics.spacing.md),
                          .isFloating = true,
                          },
        .panelStyle = {
                          .backgroundColor = colors.surfaceOverlay,
                          .borderColor = colors.borderStrong,
                          .borderWidth = metrics.borderWidth.thin,
                          .borderRadius = metrics.radius.lg,
                          .shadowColor = colors.shadow,
                          .shadowOffset = Vec2(0.0f, metrics.spacing.sm),
                          .shadowBlurRadius = metrics.spacing.xl,
                          .ignoreClip = true,
                          .zIndex = 32,
                          },
    };

    defaultConfig.wrapperLayout.combine(config.wrapperLayout);
    defaultConfig.swatchLayout.combine(config.swatchLayout);
    defaultConfig.swatchStyle.combine(config.swatchStyle);
    defaultConfig.panelLayout.combine(config.panelLayout);
    defaultConfig.panelStyle.combine(config.panelStyle);

    UIInputState result;
    UINodeState wrapper = openContainer(defaultConfig.wrapperLayout, {}, config.key);
    UIStateFlag open = UIStateStore::get().flag(wrapper.persistentKey, "pickerOpen");

    UIContainerStyleSpec swatchStyle = defaultConfig.swatchStyle;
    swatchStyle.backgroundColor = color;
    UINodeState swatch = openContainer(defaultConfig.swatchLayout, swatchStyle);
    closeContainer();

    if (swatch.isPressed) open = !open;

    if (open) {
        // Anchored off the swatch's own solved height rather than the configured one, so
        // a caller-resized swatch still drops the panel flush under it. Unscaled, since
        // that height is in real pixels while a floating offset is in design units.
        defaultConfig.panelLayout.floating =
            UIFloatingConfig {.offset = Vec2(0.0f, unscale(swatch.size.y)) + config.panelOffset};

        UINodeState panel = openContainer(defaultConfig.panelLayout, defaultConfig.panelStyle);
        result = colorPicker(color, config.pickerConfig);
        closeContainer();

        if (Input::get().uncaptured().mouseButtonPressed(MouseButton::Left) && !panel.isHovered &&
            !swatch.isHovered) {
            open = false;
        }
    }

    closeContainer();

    // The inner picker's flags with the popup's own box: a caller holds the swatch, not
    // the panel, and the panel does not exist at all while the popup is shut.
    result.node = wrapper;
    return result;
}

}   // namespace UIWidgets

}   // namespace Engine
