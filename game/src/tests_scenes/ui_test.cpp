#include "EngineInclude.hpp"
#include "test_funcs.hpp"

using namespace Engine;

namespace {

// Any .ttf/.otf dropped in here is picked up; the system font is only a fallback
// so the scene still runs on a fresh clone with no font asset committed.
const char* FONT_FOLDER = "game/assets/fonts";
const char* FALLBACK_FONT = "C:/Windows/Fonts/segoeui.ttf";

fs::path findFontFile()
{
    if (FileManager::get().doesPathExist(FONT_FOLDER)) {
        for (const auto& entry : fs::directory_iterator(FONT_FOLDER)) {
            std::string ext = entry.path().extension().string();
            if (ext == ".ttf" || ext == ".otf") return entry.path();
        }
    }

    LOG_WARNING("no .ttf/.otf found in {}; falling back to {}", FONT_FOLDER, FALLBACK_FONT);
    return FALLBACK_FONT;
}

void logFont(const Font& font, const char* label)
{
    if (!font.isValid()) {
        LOG_ERROR("{} font failed to load", label);
        return;
    }

    const FontMetrics& metrics = font.getMetrics();
    LOG_INFO(
        "{}: {} glyphs, atlas {}x{}, unitRange ({}, {}), lineHeight {} em, "
        "ascender {} em, descender {} em",
        label, font.getGlyphCount(), font.getAtlasSize().x, font.getAtlasSize().y,
        font.getUnitRange().x, font.getUnitRange().y, metrics.lineHeight, metrics.ascender,
        metrics.descender
    );

    const Glyph* glyph = font.getGlyph('A');
    if (glyph) {
        LOG_INFO(
            "  'A' advance {} em, quad ({}, {})-({}, {}), uv ({}, {})-({}, {})", glyph->advance,
            glyph->quadMin.x, glyph->quadMin.y, glyph->quadMax.x, glyph->quadMax.y, glyph->uvMin.x,
            glyph->uvMin.y, glyph->uvMax.x, glyph->uvMax.y
        );
    }
    LOG_INFO(
        "  kerning A/V {} em, distanceRange {} px, max outline/glow {} em",
        font.getKerning('A', 'V'), font.getDistanceRange(), font.getMaxEffectEm()
    );
}

}   // namespace

// Bakes one font file as both an MTSDF and a bitmap atlas, then either draws text
// through TextRenderer or shows the raw atlas texture for inspection.
// Space toggles text/atlas view, Tab switches font, 1/2/3/4 switch the atlas
// channel view, WASD/QE pan and zoom (zoom to check MTSDF edges stay crisp).
int ui_test()
{
    Logger::get().setUseAsync(true);

    IdType windowId = WindowManager::get().createWindow({900, 900, "Text Test"});

    Registry registry;
    EntityHandle camera = registry.create();
    camera.emplace<TransformComponent>();
    camera.emplace<CameraComponent>().windowId = windowId;
    camera.get<CameraComponent>().orthoSize = 2.4f;

    fs::path fontPath = findFontFile();

    // A wider distance range than the default 4 buys room for thicker outlines and
    // glows: the field can only represent offsets up to Font::getMaxEffectEm().
    Font mtsdfFont(
        fontPath, {.atlasType = FontAtlasType::Mtsdf, .emPixelSize = 48, .distanceRangePixels = 12}
    );
    Font bitmapFont(fontPath, {.atlasType = FontAtlasType::Bitmap, .emPixelSize = 16});

    logFont(mtsdfFont, "mtsdf");
    logFont(bitmapFont, "bitmap");

    if (!mtsdfFont.isValid() && !bitmapFont.isValid()) {
        LOG_ERROR("neither atlas could be built from {}; nothing to draw", fontPath);
        return 1;
    }

    GlShader atlasShader("game/assets/shaders/AtlasDebugShader.glsl");
    GlShader textShader("game/assets/shaders/TextShader.glsl");
    // Plain pass-through for the final blit instead of PostProcessingShader, whose
    // UV gradient would tint everything and make the styled colours unverifiable.
    GlShader blitShader("game/assets/shaders/QuadShader.glsl");
    Renderer::get().init(1000, &atlasShader);
    TextRenderer::get().init(&textShader);

    Input::get().addAxis("Horizontal", {KeyCode::D, KeyCode::A});
    Input::get().addAxis("Vertical", {KeyCode::W, KeyCode::S});
    Input::get().addAxis("Zoom", {KeyCode::E, KeyCode::Q});

    // The camera is 2.4 world units tall, so an em size of ~0.16 gives roughly
    // 15 lines per screen. size is in target-space units, which here are world units.
    const TextStyle base {.color = COLOR_WHITE, .size = 0.16f};
    const TextStyle red {.color = COLOR_RED, .size = 0.16f};
    const TextStyle bigCyan {.color = COLOR_CYAN, .size = 0.24f};
    const TextStyle huge {.color = COLOR_MAGENTA, .size = 0.34f};
    const TextStyle outlined {
        .color = COLOR_YELLOW, .size = 0.22f, .outlineColor = COLOR_BLUE, .outlineWidth = 0.05f
    };

    // One style per distance-field effect, so each can be judged on its own. Every
    // width is in em, so all of these hold their proportions as the camera zooms.
    const TextStyle bold {.color = COLOR_WHITE, .size = 0.16f, .boldness = 0.02f};
    const TextStyle thin {.color = COLOR_WHITE, .size = 0.16f, .boldness = -0.015f};
    const TextStyle blurred {.color = COLOR_WHITE, .size = 0.16f, .softness = 0.12f};
    const TextStyle glowing {
        .color = COLOR_WHITE,
        .size = 0.16f,
        .shadowColor = COLOR_CYAN,
        .shadowWidth = 0.05f,
        .shadowSoftness = 0.06f,
    };
    // Bright and well offset on purpose: a dark shadow one pixel behind dark text is
    // invisible for two reasons at once.
    const TextStyle shadowed {
        .color = COLOR_WHITE,
        .size = 0.16f,
        .shadowColor = COLOR_RED,
        .shadowWidth = 0.02f,
        .shadowSoftness = 0.02f,
        .shadowOffset = Vec2(0.09f, -0.09f),
    };
    const TextStyle italic {.color = COLOR_WHITE, .size = 0.16f, .italicSkew = 0.25f};
    const TextStyle underlined {.color = COLOR_WHITE, .size = 0.16f, .underline = true};
    const TextStyle struck {.color = COLOR_WHITE, .size = 0.16f, .strikethrough = true};
    const TextStyle everything {
        .color = COLOR_YELLOW,
        .size = 0.2f,
        .outlineColor = COLOR_BLUE,
        .outlineWidth = 0.05f,
        .boldness = 0.01f,
        .shadowColor = COLOR_MAGENTA,
        .shadowWidth = 0.03f,
        .shadowSoftness = 0.04f,
        .shadowOffset = Vec2(0.08f, -0.08f),
        .italicSkew = 0.18f,
        .underline = true,
    };

    // Each entry is drawn as its own block; the comment says what it proves.
    struct TextCase
    {
        const char* text;
        std::vector<TextStyle> spanStyles;
    };

    const std::vector<TextCase> cases {
        // Lines 1 and 2 both carry an oversized span while the newlines sit in
        // default-size spans: the line step must come from the tallest style on the
        // line, not from whichever span happens to hold the '\n'.
        {                    "line one /sHUGE/s here\nline two /sBIG/s too\nplain third line",{bigCyan, huge}                                                                                              },
        {                                    "default /sred span/s default /sbig cyan/s done",  {red, bigCyan}},
        {                             "short list: /sstyled/s then /sfalls back/s to default",           {red}},
        {                                   "unclosed tag: /sstyles to the end of the string",       {bigCyan}},
        {                        "escaped //s renders literally, but raw 10 m/s opens a span",           {red}},
        // Explicit UTF-8 bytes, not literal characters: without /utf-8 MSVC reads a
        // source literal in the system codepage, and whether that round-trips back to
        // UTF-8 is luck. These bytes are what the decoder must see either way.
        // u=C3BC e=C3A9 o=C3B6 c=C3A7 ss=C39F micro=C2B5 half=C2BD
        { "latin-1 via utf-8: \xc3\xbc \xc3\xa9 \xc3\xb6 \xc3\xa7 \xc3\x9f \xc2\xb5 \xc2\xbd",              {}},
        {                                           "outline set on both font types: /sABC/s",      {outlined}},
        {                                                            "tabs:\tone\ttwo\tthree",              {}},
        // One effect per span, all in one string, so they share a batch too.
        {"fx: /sbold/s /sthin/s /sblur/s /sglow/s /sshadow/s /sitalic/s /sunder/s /sstrike/s",
         {bold, thin, blurred, glowing, shadowed, italic, underlined, struck}                                 },
        {                                                       "all at once: /severything/s",    {everything}},
    };

    bool showAtlas = false;
    bool showBitmap = false;
    int viewMode = 1;

    // Measure every block once and fit the camera to the result, so the whole sheet
    // is on screen whatever gets added to `cases`. Both fonts come from the same
    // file and so share metrics, which is why one set of sizes positions both.
    const float BLOCK_GAP = 0.04f;
    const float WINDOW_ASPECT = 1.0f;

    std::vector<Vec2> blockSizes;
    float totalHeight = 0.0f, widest = 0.0f;
    for (const TextCase& textCase : cases) {
        Vec2 size =
            TextRenderer::get().measure(mtsdfFont, textCase.text, base, textCase.spanStyles);
        LOG_INFO("measure ({}, {}) for \"{}\"", size.x, size.y, textCase.text);

        blockSizes.push_back(size);
        totalHeight += size.y + BLOCK_GAP;
        if (size.x > widest) widest = size.x;
    }
    // Room for the mixed-font line appended after the blocks.
    totalHeight += mtsdfFont.getLineHeight(bigCyan.size);

    const Vec2 blockOrigin(-widest * 0.5f, totalHeight * 0.5f);
    const float fittedOrthoSize = Math::max(totalHeight, widest / WINDOW_ASPECT) * 1.06f;
    camera.get<CameraComponent>().orthoSize = fittedOrthoSize;

    bool inputSettled = false;

    auto onWindowUpdate = [&](IdType id, float dt) {
        TransformComponent& transform = camera.get<TransformComponent>();
        CameraComponent& cam = camera.get<CameraComponent>();

        int hAxis = Input::get().getAxis("Horizontal");
        int vAxis = Input::get().getAxis("Vertical");
        int zoomAxis = Input::get().getAxis("Zoom");

        // GLFW reports a phantom held key for the first frames after the window
        // opens, which would drift the fitted camera before anything is touched.
        // Hold the fit until every axis reads zero once.
        if (!inputSettled) {
            if (hAxis == 0 && vAxis == 0 && zoomAxis == 0) inputSettled = true;
            cam.orthoSize = fittedOrthoSize;
            transform.position.x = 0.0f;
            transform.position.y = 0.0f;
        } else {
            transform.position.x += hAxis * dt * cam.orthoSize;
            transform.position.y += vAxis * dt * cam.orthoSize;
            cam.orthoSize -= zoomAxis * dt * cam.orthoSize;
        }

        if (Input::get().keyPressed(KeyCode::Space)) {
            showAtlas = !showAtlas;
            LOG_INFO("showing the {}", showAtlas ? "atlas" : "text");
        }
        if (Input::get().keyPressed(KeyCode::Tab)) {
            showBitmap = !showBitmap;
            LOG_INFO("using the {} font", showBitmap ? "bitmap" : "mtsdf");
        }
        if (Input::get().keyPressed(KeyCode::Key1)) viewMode = 0;
        if (Input::get().keyPressed(KeyCode::Key2)) viewMode = 1;
        if (Input::get().keyPressed(KeyCode::Key3)) viewMode = 2;
        if (Input::get().keyPressed(KeyCode::Key4)) viewMode = 3;
    };

    auto onWindowRender = [&](IdType id, float dt) {
        const Font& font = showBitmap ? bitmapFont : mtsdfFont;

        Renderer::get().beginPass();
        Renderer::get().setShader(&atlasShader);
        Renderer::get().clearColor(Color(0.1f, 0.1f, 0.12f, 1.0f));

        if (showAtlas) {
            if (font.isValid()) {
                atlasShader.setUniform<int>("uViewMode", viewMode);
                Renderer::get().addQuad(VEC2_ZERO, VEC2_ONE * 2.0f, COLOR_WHITE, font.getTexture());
            }
        } else {
            // Renderer's batch must be flushed before the text goes down, or the
            // two batches resolve in whichever order they happen to flush.
            Renderer::get().endScene();

            Vec2 origin = blockOrigin;
            for (size_t i = 0; i < cases.size(); i++) {
                TextRenderer::get().draw(font, cases[i].text, origin, base, cases[i].spanStyles);
                origin.y -= blockSizes[i].y + BLOCK_GAP;
            }

            // The last line mixes both fonts to prove a single batch carries two
            // atlases and several styles at once.
            Vec2 pen(blockOrigin.x, origin.y - mtsdfFont.getAscender(bigCyan.size));
            pen = TextRenderer::get().drawSpan(mtsdfFont, "mtsdf ", base, pen, blockOrigin.x);
            pen = TextRenderer::get().drawSpan(bitmapFont, "+ bitmap ", red, pen, blockOrigin.x);
            TextRenderer::get().drawSpan(mtsdfFont, "in one batch", bigCyan, pen, blockOrigin.x);

            TextRenderer::get().flush();
        }

        Renderer::get().beginPass();
        Renderer::get().setShader(&blitShader);
        Renderer::get().drawToWindow();
    };

    Application app(registry);
    app.onWindowUpdate().bind(&onWindowUpdate);
    app.onWindowRender().bind(&onWindowRender);
    app.run();

    return 0;
}
