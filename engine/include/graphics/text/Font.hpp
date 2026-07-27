#pragma once

#include "core/resource_management/IResource.hpp"
#include "graphics/gl_wrappers/GlTexture.hpp"
#include "utils/ImageData.hpp"
#include "utils/TypeAliases.hpp"
#include "utils/math/Vec2.hpp"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Engine {

enum class FontAtlasType : uint8_t
{
    Mtsdf = 0,
    Bitmap,
};

enum class FontCharset : uint8_t
{
    Ascii = 0,
    AsciiLatin1,
};

struct FontBakeSettings
{
    FontAtlasType atlasType = FontAtlasType::Mtsdf;
    FontCharset charset = FontCharset::AsciiLatin1;
    uint emPixelSize = 48;
    uint distanceRangePixels = 4;
};

// Positions and sizes are in em units relative to the baseline pen origin, so
// multiply them by a pixel size to get screen units. uvMin/uvMax index the atlas
// directly and need no scaling.
struct Glyph
{
    uint32_t codepoint = 0;
    float advance = 0.0f;
    Vec2 quadMin = VEC2_ZERO, quadMax = VEC2_ZERO;
    Vec2 uvMin = VEC2_ZERO, uvMax = VEC2_ONE;
};

// Em units, same convention as Glyph.
struct FontMetrics
{
    float emSize = 1.0f;
    float lineHeight = 1.0f;
    float ascender = 0.0f, descender = 0.0f;
    float underlineY = 0.0f, underlineThickness = 0.0f;
};

struct FontKerningPair
{
    uint32_t left = 0, right = 0;
    float advance = 0.0f;
};

// Everything a Font needs, in the form both the baker and the disk cache produce
// it. Move-only because of atlasImage.
struct FontData
{
    ImageData atlasImage;
    FontMetrics metrics;
    std::vector<Glyph> glyphs;
    std::vector<FontKerningPair> kerning;
    float distanceRange = 0.0f;
};

class Font : public IResource
{
  private:
    fs::path m_sourcePath;
    FontBakeSettings m_settings;
    FontMetrics m_metrics;
    GlTexture* m_texture = nullptr;
    std::unordered_map<uint32_t, Glyph> m_glyphs;
    std::unordered_map<uint64_t, float> m_kerning;
    Vec2 m_atlasSize = VEC2_ZERO;
    Vec2 m_unitRange = VEC2_ZERO;
    float m_distanceRange = 0.0f;
    bool m_isValid = false;

  public:
    // The field only encodes distances over half its range either side of the edge,
    // so effect offsets are clamped here and in TextShader.glsl -- past this an
    // outline or glow saturates and floods the whole glyph quad.
    static constexpr float MAX_FIELD_OFFSET = 0.45f;

  public:
    Font(const fs::path& fontPath);
    Font(const fs::path& fontPath, const FontBakeSettings& settings);
    ~Font() override;

    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;
    Font(Font&& other) noexcept;
    Font& operator=(Font&& other) noexcept;

    bool isValid() const { return m_isValid; }
    FontAtlasType getAtlasType() const { return m_settings.atlasType; }
    const FontBakeSettings& getBakeSettings() const { return m_settings; }
    const fs::path& getSourcePath() const { return m_sourcePath; }

    const GlTexture* getTexture() const { return m_texture; }
    Vec2 getAtlasSize() const { return m_atlasSize; }
    Vec2 getUnitRange() const { return m_unitRange; }
    float getDistanceRange() const { return m_distanceRange; }
    // The widest outline or glow the baked field can represent, in em. Anything
    // past this saturates, so bake with a larger distanceRangePixels to exceed it.
    float getMaxEffectEm() const;
    float getNativePixelSize() const { return (float)m_settings.emPixelSize; }

    const FontMetrics& getMetrics() const { return m_metrics; }
    float getLineHeight(float pixelSize) const { return m_metrics.lineHeight * pixelSize; }
    float getAscender(float pixelSize) const { return m_metrics.ascender * pixelSize; }
    float getDescender(float pixelSize) const { return m_metrics.descender * pixelSize; }

    const Glyph* getGlyph(uint32_t codepoint) const;
    bool hasGlyph(uint32_t codepoint) const;
    float getKerning(uint32_t left, uint32_t right) const;
    size_t getGlyphCount() const { return m_glyphs.size(); }

  private:
    void load();
    void adopt(FontData& data);

    static uint64_t kerningKey(uint32_t left, uint32_t right);
};

}   // namespace Engine
