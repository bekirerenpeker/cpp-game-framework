#include "graphics/text/FontBaker.hpp"
#include "core/logging/LoggerMacros.hpp"

#ifdef ENGINE_HAS_FONT_BAKER

#include <msdf-atlas-gen/msdf-atlas-gen.h>
#include <algorithm>
#include <cstring>
#include <thread>

namespace Engine {

namespace FontBaker {

// Matches msdf-atlas-gen's own defaults for edge coloring.
static constexpr double ANGLE_THRESHOLD = 3.0;
static constexpr unsigned long long COLORING_SEED = 0;
// A mask atlas carries coverage, not distance, so it only needs a 1px range.
static constexpr double MASK_PIXEL_RANGE = 1.0;

bool isAvailable() { return true; }

static void resolveCharset(FontCharset charset, msdf_atlas::Charset& out)
{
    for (uint32_t cp = 0x20; cp <= 0x7E; cp++) out.add(cp);
    if (charset == FontCharset::AsciiLatin1) {
        for (uint32_t cp = 0xA0; cp <= 0xFF; cp++) out.add(cp);
    }
}

// Copies the packed atlas into an ImageData. msdfgen bitmaps are Y_UPWARD with
// row 0 at the bottom, which is already the engine's ImageData convention (stb
// does the flip to/from top-origin PNG on write/read), so rows copy straight
// across.
template<int N>
static void
copyAtlas(const msdfgen::BitmapConstSection<msdf_atlas::byte, N>& bitmap, ImageData& out)
{
    out = ImageData((size_t)bitmap.width, (size_t)bitmap.height, (size_t)N);
    for (int y = 0; y < bitmap.height; y++) {
        std::memcpy(
            out.pixels + (size_t)y * bitmap.width * N, bitmap(0, y), (size_t)bitmap.width * N
        );
    }
}

template<int N, msdf_atlas::GeneratorFunction<float, N> GEN_FN>
static void generateAtlas(
    const std::vector<msdf_atlas::GlyphGeometry>& glyphs, int width, int height, ImageData& out
)
{
    msdf_atlas::ImmediateAtlasGenerator<
        float, N, GEN_FN, msdf_atlas::BitmapAtlasStorage<msdf_atlas::byte, N>>
        generator(width, height);

    msdf_atlas::GeneratorAttributes attributes;
    generator.setAttributes(attributes);
    generator.setThreadCount(std::max((int)std::thread::hardware_concurrency(), 1));
    generator.generate(glyphs.data(), (int)glyphs.size());

    msdfgen::BitmapConstSection<msdf_atlas::byte, N> bitmap =
        (msdfgen::BitmapConstSection<msdf_atlas::byte, N>)generator.atlasStorage();
    copyAtlas<N>(bitmap, out);
}

bool bake(const fs::path& fontPath, const FontBakeSettings& settings, FontData& outData)
{
    msdfgen::FreetypeHandle* freetype = msdfgen::initializeFreetype();
    if (!freetype) {
        LOG_ERROR("couldn't initialize freetype while baking {}", fontPath);
        return false;
    }

    msdfgen::FontHandle* font = msdfgen::loadFont(freetype, fontPath.string().c_str());
    if (!font) {
        LOG_ERROR("couldn't load font {}", fontPath);
        msdfgen::deinitializeFreetype(freetype);
        return false;
    }

    bool isMtsdf = settings.atlasType == FontAtlasType::Mtsdf;

    msdf_atlas::Charset charset;
    resolveCharset(settings.charset, charset);

    std::vector<msdf_atlas::GlyphGeometry> glyphs;
    msdf_atlas::FontGeometry fontGeometry(&glyphs);
    // A geometry scale of 1 keeps every coordinate in em units.
    int loadedCount = fontGeometry.loadCharset(font, 1.0, charset, true, true);

    msdfgen::destroyFont(font);
    msdfgen::deinitializeFreetype(freetype);

    if (loadedCount <= 0 || glyphs.empty()) {
        LOG_ERROR("no glyphs could be loaded from {}", fontPath);
        return false;
    }
    if ((size_t)loadedCount < charset.size()) {
        LOG_WARNING(
            "{} only provided {} of the {} requested glyphs", fontPath, loadedCount, charset.size()
        );
    }

    if (isMtsdf) {
        unsigned long long glyphSeed = COLORING_SEED;
        for (msdf_atlas::GlyphGeometry& glyph : glyphs) {
            glyph.edgeColoring(&msdfgen::edgeColoringInkTrap, ANGLE_THRESHOLD, glyphSeed);
        }
    }

    double pixelRange = isMtsdf ? (double)settings.distanceRangePixels : MASK_PIXEL_RANGE;

    msdf_atlas::TightAtlasPacker packer;
    packer.setDimensionsConstraint(msdf_atlas::DimensionsConstraint::POWER_OF_TWO_SQUARE);
    packer.setScale((double)settings.emPixelSize);
    packer.setPixelRange(msdfgen::Range(pixelRange));
    // Only the multi-channel types have miters to limit; masks must pass 0.
    packer.setMiterLimit(isMtsdf ? 1.0 : 0.0);

    if (packer.pack(glyphs.data(), (int)glyphs.size()) != 0) {
        LOG_ERROR("couldn't pack a font atlas for {}", fontPath);
        return false;
    }

    int width = 0, height = 0;
    packer.getDimensions(width, height);
    if (width <= 0 || height <= 0) {
        LOG_ERROR("font atlas for {} packed to an empty size", fontPath);
        return false;
    }

    // Passed without a leading & -- MSVC mishandles an address-of expression as a
    // function-pointer template argument here and fails inside the generator.
    if (isMtsdf)
        generateAtlas<4, msdf_atlas::mtsdfGenerator>(glyphs, width, height, outData.atlasImage);
    else generateAtlas<1, msdf_atlas::sdfGenerator>(glyphs, width, height, outData.atlasImage);

    msdfgen::Range bakedRange = packer.getPixelRange();
    outData.distanceRange = (float)(bakedRange.upper - bakedRange.lower);

    const msdfgen::FontMetrics& srcMetrics = fontGeometry.getMetrics();
    outData.metrics.emSize = (float)srcMetrics.emSize;
    outData.metrics.lineHeight = (float)srcMetrics.lineHeight;
    outData.metrics.ascender = (float)srcMetrics.ascenderY;
    outData.metrics.descender = (float)srcMetrics.descenderY;
    outData.metrics.underlineY = (float)srcMetrics.underlineY;
    outData.metrics.underlineThickness = (float)srcMetrics.underlineThickness;

    outData.glyphs.clear();
    outData.glyphs.reserve(glyphs.size());
    for (const msdf_atlas::GlyphGeometry& glyph : glyphs) {
        Glyph out;
        out.codepoint = glyph.getCodepoint();
        out.advance = (float)glyph.getAdvance();

        // Whitespace has no box; leaving the quad and UVs zeroed marks it as
        // advance-only so a layout pass can skip emitting geometry for it.
        if (!glyph.isWhitespace()) {
            double l = 0, b = 0, r = 0, t = 0;
            glyph.getQuadPlaneBounds(l, b, r, t);
            out.quadMin = Vec2((float)l, (float)b);
            out.quadMax = Vec2((float)r, (float)t);

            glyph.getQuadAtlasBounds(l, b, r, t);
            out.uvMin = Vec2((float)(l / width), (float)(b / height));
            out.uvMax = Vec2((float)(r / width), (float)(t / height));
        }

        outData.glyphs.push_back(out);
    }

    // getKerning() is keyed by glyph index, but a layout pass only ever has
    // codepoints, so remap here while the geometry is still around to ask.
    outData.kerning.clear();
    for (const auto& [indexPair, advance] : fontGeometry.getKerning()) {
        const msdf_atlas::GlyphGeometry* left =
            fontGeometry.getGlyph(msdfgen::GlyphIndex(indexPair.first));
        const msdf_atlas::GlyphGeometry* right =
            fontGeometry.getGlyph(msdfgen::GlyphIndex(indexPair.second));
        if (!left || !right || !left->getCodepoint() || !right->getCodepoint()) continue;

        FontKerningPair pair;
        pair.left = left->getCodepoint();
        pair.right = right->getCodepoint();
        pair.advance = (float)advance;
        outData.kerning.push_back(pair);
    }

    LOG_INFO(
        "baked {} ({} glyphs, {} kerning pairs) into a {}x{} {} atlas", fontPath,
        outData.glyphs.size(), outData.kerning.size(), width, height, isMtsdf ? "mtsdf" : "bitmap"
    );
    return true;
}

}   // namespace FontBaker

}   // namespace Engine

#else   // ENGINE_HAS_FONT_BAKER

namespace Engine {

namespace FontBaker {

bool isAvailable() { return false; }

bool bake(const fs::path& fontPath, const FontBakeSettings& settings, FontData& outData)
{
    LOG_ERROR(
        "can't bake {} because the font baker was compiled out; "
        "reconfigure with ENGINE_BUILD_FONT_BAKER=ON or ship a prebuilt .cache/fonts entry",
        fontPath
    );
    return false;
}

}   // namespace FontBaker

}   // namespace Engine

#endif   // ENGINE_HAS_FONT_BAKER
