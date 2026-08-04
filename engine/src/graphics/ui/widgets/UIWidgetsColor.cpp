#include "graphics/ui/UIWidgets.hpp"
#include "core/file_management/FileManager.hpp"
#include "core/input/Input.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "graphics/gl_wrappers/GlShader.hpp"
#include "graphics/ui/UiManager.hpp"
#include "utils/math/MathFuncs.hpp"
#include <format>
#include <string>
#include <unordered_map>

namespace Engine {

namespace UIWidgets {

namespace {

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

// Hue and saturation have no meaning in a black or greyscale RGB value, so they are
// kept here per picker instead of being re-derived every frame and lost.
std::unordered_map<const Color*, Vec3> g_pickerHsv;
std::unordered_map<const Color*, bool> g_pickerOpen;

}   // namespace

void colorPicker(Color& color, const ColorPickerConfig& config)
{
    ColorPickerConfig defaultConfig = {
        .pickerLayout =
            {
                           .width = UISizeSpec::grow(),
                           .gap = 8.0f,
                           .direction = UILayoutDirection::Column,
                           },
        .pickerStyle = {},
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
                           .borderColor = COLOR_WHITE,
                           .borderWidth = 2.0f,
                           .borderRadius = 6.0f,
                           .shadowColor = Color(0.0f, 0.0f, 0.0f, 0.8f),
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
                           .backgroundColor = COLOR_WHITE,
                           .borderColor = Color(0.0f, 0.0f, 0.0f, 0.6f),
                           .borderWidth = 1.0f,
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
                           .backgroundColor = COLOR_WHITE,
                           .borderColor = Color(0.0f, 0.0f, 0.0f, 0.6f),
                           .borderWidth = 1.0f,
                           .borderRadius = 2.0f,
                           },
        .previewLayout =
            {
                           .width = UISizeSpec::grow(),
                           .height = UISizeSpec::fixed(22.0f),
                           },
        .previewStyle =
            {
                           .borderColor = Color(0.38f, 0.42f, 0.52f),
                           .borderWidth = 1.0f,
                           .borderRadius = 4.0f,
                           },
        .valuesLayout =
            {
                           .width = UISizeSpec::grow(),
                           .gap = 2.0f,
                           .direction = UILayoutDirection::Column,
                           },
        .valuesTextConfig = {.style = {.color = Color(0.66f, 0.70f, 0.78f), .size = 11.0f}},
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

    Vec3& hsv = g_pickerHsv.try_emplace(&color, color.toHsv()).first->second;
    // The caller may have written the colour itself since last frame; only then is the
    // stored hue thrown away, so dragging to black does not lose it.
    Color fromState = Color::fromHsv(hsv, color.a);
    if (Math::abs(fromState.r - color.r) > 0.001f || Math::abs(fromState.g - color.g) > 0.001f ||
        Math::abs(fromState.b - color.b) > 0.001f) {
        hsv = color.toHsv();
    }

    float squareHeight = config.squareHeight > 0.0f ? config.squareHeight : 120.0f;

    openContainer(defaultConfig.pickerLayout, defaultConfig.pickerStyle, config.key);

    openContainer({
        .width = UISizeSpec::grow(),
        .height = UISizeSpec::fixed(squareHeight),
        .gap = 8.0f,
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
    if (square.isActive || squareAnchor.isActive || squareMarker.isActive) {
        hsv.y = Math::clamp(square.relativeMousePos.x, 0.0f, 1.0f);
        hsv.z = 1.0f - Math::clamp(square.relativeMousePos.y, 0.0f, 1.0f);
    }
    if (hueStrip.isActive || hueAnchor.isActive || hueMarker.isActive)
        hsv.x = Math::clamp(hueStrip.relativeMousePos.y, 0.0f, 0.9999f);
    if (alphaStrip.isActive || alphaAnchor.isActive || alphaMarker.isActive)
        alpha = Math::clamp(alphaStrip.relativeMousePos.x, 0.0f, 1.0f);

    color = Color::fromHsv(hsv, alpha);

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
}

void colorPickerPopup(Color& color, const ColorPickerPopupConfig& config)
{
    ColorPickerPopupConfig defaultConfig = {
        .wrapperLayout = {},
        .swatchLayout =
            {
                          .width = UISizeSpec::fixed(46.0f),
                          .height = UISizeSpec::fixed(22.0f),
                          },
        .swatchStyle =
            {
                          .borderColor = Color(0.38f, 0.42f, 0.52f),
                          .borderWidth = 1.0f,
                          .borderRadius = 4.0f,
                          .onHover = {.borderColor = COLOR_WHITE},
                          },
        .panelLayout =
            {
                          .width = UISizeSpec::fixed(220.0f),
                          .padding = UIEdges(10.0f),
                          .isFloating = true,
                          },
        .panelStyle = {
                          .backgroundColor = Color(0.14f, 0.15f, 0.20f),
                          .borderColor = Color(0.34f, 0.38f, 0.48f),
                          .borderWidth = 1.0f,
                          .borderRadius = 8.0f,
                          .shadowColor = Color(0.0f, 0.0f, 0.0f, 0.7f),
                          .shadowOffset = Vec2(0.0f, 6.0f),
                          .shadowBlurRadius = 20.0f,
                          .ignoreClip = true,
                          .zIndex = 32,
                          },
    };

    defaultConfig.wrapperLayout.combine(config.wrapperLayout);
    defaultConfig.swatchLayout.combine(config.swatchLayout);
    defaultConfig.swatchStyle.combine(config.swatchStyle);
    defaultConfig.panelLayout.combine(config.panelLayout);
    defaultConfig.panelStyle.combine(config.panelStyle);

    bool& open = g_pickerOpen[&color];

    openContainer(defaultConfig.wrapperLayout, {}, config.key);

    UIContainerStyleSpec swatchStyle = defaultConfig.swatchStyle;
    swatchStyle.backgroundColor = color;
    UINodeState swatch = openContainer(defaultConfig.swatchLayout, swatchStyle);
    closeContainer();

    if (swatch.isPressed) open = !open;

    if (open) {
        // Anchored off the swatch's own solved height rather than the configured one, so
        // a caller-resized swatch still drops the panel flush under it.
        defaultConfig.panelLayout.floating =
            UIFloatingConfig {.offset = Vec2(0.0f, swatch.size.y) + config.panelOffset};

        UINodeState panel = openContainer(defaultConfig.panelLayout, defaultConfig.panelStyle);
        colorPicker(color, config.pickerConfig);
        closeContainer();

        if (Input::get().mouseButtonPressed(MouseButton::Left) && !panel.isHovered &&
            !swatch.isHovered) {
            open = false;
        }
    }

    closeContainer();
}


}   // namespace UIWidgets

}   // namespace Engine
