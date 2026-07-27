#include "graphics/text/Font.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "graphics/text/FontBaker.hpp"
#include "graphics/text/FontCache.hpp"

namespace Engine {

Font::Font(const fs::path& fontPath) : m_sourcePath(fontPath) { load(); }

Font::Font(const fs::path& fontPath, const FontBakeSettings& settings)
    : m_sourcePath(fontPath), m_settings(settings)
{
    load();
}

Font::~Font() { delete m_texture; }

Font::Font(Font&& other) noexcept
    : m_sourcePath(std::move(other.m_sourcePath)),
      m_settings(other.m_settings),
      m_metrics(other.m_metrics),
      m_texture(other.m_texture),
      m_glyphs(std::move(other.m_glyphs)),
      m_kerning(std::move(other.m_kerning)),
      m_atlasSize(other.m_atlasSize),
      m_unitRange(other.m_unitRange),
      m_isValid(other.m_isValid)
{
    other.m_texture = nullptr;
    other.m_isValid = false;
}

Font& Font::operator=(Font&& other) noexcept
{
    if (this == &other) return *this;

    delete m_texture;

    m_sourcePath = std::move(other.m_sourcePath);
    m_settings = other.m_settings;
    m_metrics = other.m_metrics;
    m_texture = other.m_texture;
    m_glyphs = std::move(other.m_glyphs);
    m_kerning = std::move(other.m_kerning);
    m_atlasSize = other.m_atlasSize;
    m_unitRange = other.m_unitRange;
    m_isValid = other.m_isValid;

    other.m_texture = nullptr;
    other.m_isValid = false;
    return *this;
}

void Font::load()
{
    FontCacheKey key;
    if (!FontCache::buildKey(m_sourcePath, m_settings, key)) return;

    FontData data;
    if (FontCache::load(key, m_settings, data)) {
        LOG_INFO("loaded the cached font atlas for {}", m_sourcePath);
        adopt(data);
        return;
    }

    if (!FontBaker::bake(m_sourcePath, m_settings, data)) return;

    if (FontCache::store(key, m_settings, data)) FontCache::evictStale(key);
    adopt(data);
}

void Font::adopt(FontData& data)
{
    m_metrics = data.metrics;

    m_glyphs.clear();
    m_glyphs.reserve(data.glyphs.size());
    for (const Glyph& glyph : data.glyphs) m_glyphs[glyph.codepoint] = glyph;

    m_kerning.clear();
    m_kerning.reserve(data.kerning.size());
    for (const FontKerningPair& pair : data.kerning) {
        m_kerning[kerningKey(pair.left, pair.right)] = pair.advance;
    }

    m_atlasSize = Vec2((float)data.atlasImage.width, (float)data.atlasImage.height);

    // Linear filtering is mandatory for a distance field -- point sampling would
    // reduce it to a hard-edged mask. A bitmap atlas wants the opposite, since its
    // whole reason for existing is staying crisp at its native size.
    bool isMtsdf = m_settings.atlasType == FontAtlasType::Mtsdf;
    GlTexture::FilterMode filterMode =
        isMtsdf ? GlTexture::FilterMode::Linear : GlTexture::FilterMode::Point;

    delete m_texture;
    m_texture = new GlTexture(data.atlasImage, filterMode, GlTexture::WrapMode::ClampToEdge);

    // The per-vertex factor an msdf shader needs to convert distances into screen
    // pixels. Zero for a bitmap atlas, which doubles as "this is not a field".
    m_distanceRange = isMtsdf ? data.distanceRange : 0.0f;
    if (isMtsdf && m_atlasSize.x > 0 && m_atlasSize.y > 0) {
        m_unitRange = Vec2(data.distanceRange / m_atlasSize.x, data.distanceRange / m_atlasSize.y);
    } else {
        m_unitRange = VEC2_ZERO;
    }

    m_isValid = true;
}

// The field encodes distances over +-distanceRange/2 atlas pixels, so an offset
// past half the normalized range saturates everywhere and would flood the glyph
// quad. MAX_FIELD_OFFSET is the shader's clamp expressed back in em.
float Font::getMaxEffectEm() const
{
    if (m_settings.emPixelSize == 0 || m_distanceRange <= 0.0f) return 0.0f;
    return MAX_FIELD_OFFSET * m_distanceRange / (float)m_settings.emPixelSize;
}

uint64_t Font::kerningKey(uint32_t left, uint32_t right)
{
    return ((uint64_t)left << 32) | (uint64_t)right;
}

const Glyph* Font::getGlyph(uint32_t codepoint) const
{
    auto it = m_glyphs.find(codepoint);
    return it == m_glyphs.end() ? nullptr : &it->second;
}

bool Font::hasGlyph(uint32_t codepoint) const { return m_glyphs.contains(codepoint); }

float Font::getKerning(uint32_t left, uint32_t right) const
{
    auto it = m_kerning.find(kerningKey(left, right));
    return it == m_kerning.end() ? 0.0f : it->second;
}

}   // namespace Engine
