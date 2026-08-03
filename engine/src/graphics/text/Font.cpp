#include "graphics/text/Font.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "graphics/text/FontLoader.hpp"
#include <thread>

namespace Engine {

namespace {

// What every codepoint resolves to until the real atlas arrives. The box is roughly
// the shape of a lowercase glyph so a loading paragraph wraps and stacks about where
// the finished one will, and its uv covers the whole texture, which is the batch
// renderer's 1x1 white default while getTexture() is null -- so it fills solid.
const Glyph PLACEHOLDER_GLYPH {
    .advance = 0.55f, .quadMin = Vec2(0.06f, 0.0f), .quadMax = Vec2(0.5f, 0.66f)
};

// A zero-area box: appendGlyph skips it, so words stay words instead of the whole
// line running together into one bar.
const Glyph PLACEHOLDER_SPACE {.advance = 0.28f};

constexpr FontMetrics PLACEHOLDER_METRICS {
    .emSize = 1.0f,
    .lineHeight = 1.2f,
    .ascender = 0.8f,
    .descender = -0.2f,
    .underlineY = -0.1f,
    .underlineThickness = 0.05f
};

bool isSpacing(uint32_t codepoint)
{
    return codepoint == ' ' || codepoint == '\t' || codepoint == '\n' || codepoint == '\r';
}

}   // namespace

Font::Font(const fs::path& fontPath) : m_sourcePath(fontPath) { beginLoad(); }

Font::Font(const fs::path& fontPath, const FontBakeSettings& settings)
    : m_sourcePath(fontPath), m_settings(settings)
{
    beginLoad();
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
      m_distanceRange(other.m_distanceRange),
      m_job(std::move(other.m_job)),
      m_state(other.m_state),
      m_loadVersion(other.m_loadVersion)
{
    other.m_texture = nullptr;
    other.m_state = FontLoadState::Failed;
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
    m_distanceRange = other.m_distanceRange;
    m_job = std::move(other.m_job);
    m_state = other.m_state;
    m_loadVersion = other.m_loadVersion;

    other.m_texture = nullptr;
    other.m_state = FontLoadState::Failed;
    return *this;
}

void Font::beginLoad()
{
    m_metrics = PLACEHOLDER_METRICS;
    m_job = FontLoader::get().submit(m_sourcePath, m_settings);
}

// Called from the const accessors, so a Font needs no update() and no owner -- the
// first query after the worker is done installs the result. The const_cast is safe
// since every caller still sees the same font, just backed by the real atlas now.
void Font::pollLoad() const
{
    if (m_state != FontLoadState::Loading) return;
    if (!m_job || !m_job->isDone.load(std::memory_order_acquire)) return;

    const_cast<Font*>(this)->finishLoad();
}

// Runs on whichever thread polled, which is the one that owns the GL context, because
// adopt() uploads the atlas. That is the entire reason the worker stops at FontData.
void Font::finishLoad()
{
    if (m_job->succeeded) {
        adopt(m_job->data);
    } else {
        m_state = FontLoadState::Failed;
        LOG_ERROR("failed to load the font {}", m_sourcePath);
    }

    m_job.reset();
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

    m_state = FontLoadState::Ready;

    // Bumped so a TextBlock solved against the placeholder knows to lay itself out
    // again; nothing else can tell that the glyphs under it changed.
    m_loadVersion++;
}

bool Font::isValid() const
{
    pollLoad();
    return m_state != FontLoadState::Failed;
}

bool Font::isLoading() const
{
    pollLoad();
    return m_state == FontLoadState::Loading;
}

bool Font::isReady() const
{
    pollLoad();
    return m_state == FontLoadState::Ready;
}

uint Font::getLoadVersion() const
{
    pollLoad();
    return m_loadVersion;
}

void Font::waitForLoad()
{
    if (m_state != FontLoadState::Loading || !m_job) return;

    // Yielding rather than a condition variable keeps the job free of anything the
    // worker has to signal; this is the deliberately rare path.
    while (!m_job->isDone.load(std::memory_order_acquire)) std::this_thread::yield();
    finishLoad();
}

// The field encodes distances over +-distanceRange/2 atlas pixels; MAX_FIELD_OFFSET
// is the shader's saturation clamp expressed back in em.
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
    pollLoad();
    if (m_state == FontLoadState::Loading) {
        return isSpacing(codepoint) ? &PLACEHOLDER_SPACE : &PLACEHOLDER_GLYPH;
    }

    auto it = m_glyphs.find(codepoint);
    return it == m_glyphs.end() ? nullptr : &it->second;
}

bool Font::hasGlyph(uint32_t codepoint) const
{
    pollLoad();
    if (m_state == FontLoadState::Loading) return true;
    return m_glyphs.contains(codepoint);
}

float Font::getKerning(uint32_t left, uint32_t right) const
{
    auto it = m_kerning.find(kerningKey(left, right));
    return it == m_kerning.end() ? 0.0f : it->second;
}

}   // namespace Engine
