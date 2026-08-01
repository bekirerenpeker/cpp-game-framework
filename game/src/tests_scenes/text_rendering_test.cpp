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

void logLines(const TextBlock& block, const char* label)
{
    const std::vector<TextLine>& lines = block.getLines();
    LOG_INFO(
        "  {}: {} lines, bounds ({}, {})", label, lines.size(), block.getBounds().x,
        block.getBounds().y
    );
    for (size_t i = 0; i < lines.size(); i++) {
        LOG_INFO(
            "    line {}: top {} width {} height {} ascent {} descent {} runs {}", i, lines[i].top,
            lines[i].width, lines[i].height, lines[i].ascent, lines[i].descent, lines[i].runCount
        );
    }
}

// Everything the fold is supposed to guarantee, logged once at startup so it can be
// checked from game/output/log.txt without driving the window.
void logLayoutProof(
    const Font& font, const TextStyle& base, const TextStyle& bigCyan, const TextStyle& huge
)
{
    TextLayoutCalculator& calc = TextLayoutCalculator::get();

    LOG_INFO("=== line box: a line is as tall as the tallest style touching it ===");
    TextBlock mixed(&font, "line one /sHUGE/s here\nline two /sBIG/s too\nplain third line", base);
    mixed.setSpanStyles({bigCyan, huge});
    calc.calculate(mixed, 0.0f);
    logLines(mixed, "per-style line heights");

    // The baseline must sit at lineTop + that line's own ascent. Stepping
    // baseline-to-baseline instead is what used to let a tall line 2 ride up into
    // line 1, so a non-negative gap here is the regression check.
    const std::vector<TextLine>& ml = mixed.getLines();
    for (size_t i = 0; i + 1 < ml.size(); i++) {
        float gap = ml[i].height - ml[i].ascent - ml[i].descent;
        LOG_INFO("    gap below line {}: {} (must be >= 0, no overlap)", i, gap);
    }

    mixed.setFixedLineHeight(true);
    calc.calculate(mixed, 0.0f);
    logLines(mixed, "fixedLineHeight opts out, all steps equal");
    mixed.setFixedLineHeight(false);

    LOG_INFO("=== wrapping: world-space text can wrap now ===");
    TextBlock para(
        &font,
        "the quick brown fox jumps over the lazy dog while a col/sour/sful span "
        "straddles a tag boundary and must not break there",
        base
    );
    para.setSpanStyles({bigCyan});

    TextBlockWidths widths = calc.measureMinMaxWidth(para);
    LOG_INFO(
        "  min-content (longest word) {}, max-content (longest line) {}", widths.min, widths.max
    );

    for (float divisor : {1.0f, 2.0f, 4.0f, 12.0f}) {
        calc.calculate(para, widths.max / divisor);
        LOG_INFO(
            "  width {} -> {} lines, bounds ({}, {})", widths.max / divisor, para.getLines().size(),
            para.getBounds().x, para.getBounds().y
        );
    }

    // Non-wrapping text is incompressible, so its min must collapse onto its max.
    para.setWrapEnabled(false);
    TextBlockWidths noWrap = calc.measureMinMaxWidth(para);
    LOG_INFO("  wrap disabled -> min {} max {} (must be equal)", noWrap.min, noWrap.max);
    para.setWrapEnabled(true);

    LOG_INFO("=== alignment: per line, not per block ===");
    TextBlock aligned(&font, "a short line\nand a considerably longer second line", base);
    const float alignWidth = 3.0f;
    for (auto [name, mode] : {
             std::pair {  "Left",   TextAlignH::Left},
             {"Center", TextAlignH::Center},
             { "Right",  TextAlignH::Right}
    }) {
        aligned.setAlignH(mode);
        calc.calculate(aligned, alignWidth);
        for (const TextLine& line : aligned.getLines()) {
            LOG_INFO(
                "  {} width {} -> offset.x {} (expected {})", name, line.width,
                aligned.getRuns()[line.firstRun].offset.x,
                mode == TextAlignH::Left   ? 0.0f :
                mode == TextAlignH::Center ? (alignWidth - line.width) * 0.5f :
                                             alignWidth - line.width
            );
        }
    }

    LOG_INFO("=== cache + invalidation ===");
    TextBlock cached(&font, "cache probe", base);
    LOG_INFO("  fresh block dirty: {} (expected true)", cached.isDirty());
    calc.calculate(cached, 2.0f);
    LOG_INFO("  after calculate dirty: {} (expected false)", cached.isDirty());
    LOG_INFO("  same width needs rework: {} (expected false)", cached.isDirtyFor(2.0f));
    LOG_INFO("  new width needs rework: {} (expected true)", cached.isDirtyFor(1.0f));
    cached.setAlignV(TextAlignV::Middle);
    LOG_INFO("  after setAlignV dirty: {} (expected false, applied at draw)", cached.isDirty());
    cached.setAlignH(TextAlignH::Center);
    LOG_INFO("  after setAlignH dirty: {} (expected true)", cached.isDirty());
    calc.calculate(cached, 2.0f);
    cached.setText("a different string");
    LOG_INFO(
        "  after setText dirty: {}, runs cleared: {} (both expected true)", cached.isDirty(),
        cached.getRuns().empty()
    );
}

}   // namespace

// Bakes one font file as both an MTSDF and a bitmap atlas, then either draws text
// through TextRenderer or shows the raw atlas texture for inspection.
// Space toggles text/atlas view, Tab switches font, 1/2/3/4 switch the atlas
// channel view, WASD/QE pan and zoom (zoom to check MTSDF edges stay crisp).
int text_rendering_test()
{
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

    // TextBlock owns its string and holds views into it, so it is deliberately
    // neither copyable nor movable -- hence pointers rather than a vector of values.
    std::vector<TextBlock*> blocks;
    float totalHeight = 0.0f, widest = 0.0f;
    for (const TextCase& textCase : cases) {
        TextBlock* block = new TextBlock(&mtsdfFont, textCase.text, base);
        block->setSpanStyles(textCase.spanStyles);

        // An unbounded width breaks only on explicit newlines, which is the
        // shrink-to-fit measurement this camera fit wants.
        TextLayoutCalculator::get().calculate(*block, 0.0f);
        Vec2 size = block->getBounds();
        LOG_INFO("bounds ({}, {}) for \"{}\"", size.x, size.y, textCase.text);

        blocks.push_back(block);
        totalHeight += size.y + BLOCK_GAP;
        if (size.x > widest) widest = size.x;
    }

    logLayoutProof(mtsdfFont, base, bigCyan, huge);

    // Async only from here: the startup proof above has to survive a hard kill, and
    // the queue is what the frame loop actually needs.
    Logger::get().setUseAsync(true);
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
            for (TextBlock* block : blocks) {
                // Tab swaps the atlas. setFont early-returns when unchanged and
                // calculate early-outs on a clean block, so the steady state costs
                // nothing and only an actual swap re-walks the strings.
                block->setFont(&font);
                TextLayoutCalculator::get().calculate(*block, 0.0f);

                TextRenderer::get().draw(*block, origin);
                origin.y -= block->getBounds().y + BLOCK_GAP;
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

    for (TextBlock* block : blocks) delete block;
    return 0;
}
