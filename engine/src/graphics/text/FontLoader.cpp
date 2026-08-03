#include "graphics/text/FontLoader.hpp"
#include "core/file_management/FileManager.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "graphics/text/FontBaker.hpp"
#include "graphics/text/FontCache.hpp"

#if OS_NAME == OS_WINDOWS
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace Engine {

namespace {

// Tried in order for the default font. Arial is the one face a stock Windows and macOS
// both have; the Linux entries are what the common distros ship in its place.
const char* SYSTEM_FONT_CANDIDATES[] = {
#if OS_NAME == OS_WINDOWS
    "C:/Windows/Fonts/arial.ttf",
    "C:/Windows/Fonts/segoeui.ttf",
    "C:/Windows/Fonts/tahoma.ttf",
#elif OS_NAME == OS_MACOS
    "/System/Library/Fonts/Supplemental/Arial.ttf",
    "/Library/Fonts/Arial.ttf",
    "/System/Library/Fonts/Supplemental/Verdana.ttf",
#else
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
#endif
};

// Bitmap, and small: this one is on the critical path of every other font, since they
// all borrow it while they bake. A bitmap atlas is roughly twenty times cheaper per
// glyph than an mtsdf one, which is what makes "queued first" also mean "ready first".
constexpr FontBakeSettings DEFAULT_FONT_BAKE {
    .atlasType = FontAtlasType::Bitmap, .charset = FontCharset::AsciiLatin1, .emPixelSize = 32
};

}   // namespace

FontLoader::~FontLoader()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_isRunning = false;
    }
    m_wakeup.notify_all();

    // Whatever is mid-bake is waited out -- msdf-atlas-gen has no cancel and the job is
    // shared state a Font may still be polling. Queued jobs are simply dropped.
    if (m_worker.joinable()) m_worker.join();

    delete m_defaultFont;
}

// Created by the first submit rather than at engine init, so it is queued ahead of the
// font that triggered it and nothing bakes at all in a scene with no text. The request
// flag is set before the Font is built because that constructor submits, which lands
// straight back here.
//
// Default-font state is main-thread only: Fonts are constructed there, and resolve() is
// called from layout and drawing, which are too.
void FontLoader::ensureDefaultFont()
{
    if (m_defaultRequested) return;
    m_defaultRequested = true;

    for (const char* candidate : SYSTEM_FONT_CANDIDATES) {
        if (!FileManager::get().doesPathExist(candidate)) continue;

        LOG_INFO("baking {} as the default font", candidate);
        m_defaultFont = new Font(candidate, DEFAULT_FONT_BAKE);
        return;
    }

    LOG_ERROR(
        "no system font found for the default font; text with no font of its own "
        "will draw as placeholder boxes"
    );
}

const Font* FontLoader::getDefaultFont()
{
    ensureDefaultFont();
    return m_defaultFont;
}

const Font* FontLoader::resolve(const Font* requested)
{
    if (!requested) return getDefaultFont();
    if (!requested->isLoading()) return requested;

    const Font* fallback = getDefaultFont();
    return fallback && fallback->isReady() ? fallback : requested;
}

// One worker rather than a pool: a bake is msdf-atlas-gen plus a PNG write, and running
// two at once would need the baker re-entrant for no real gain, since fonts arrive a
// handful at a time. Sequential and off the main thread is the whole requirement.
std::shared_ptr<FontLoadJob>
FontLoader::submit(const fs::path& sourcePath, const FontBakeSettings& settings)
{
    // Before the job is queued, so the default font's own job goes in first and every
    // other font has something real to borrow as early as possible.
    ensureDefaultFont();

    std::shared_ptr<FontLoadJob> job = std::make_shared<FontLoadJob>();
    job->sourcePath = sourcePath;
    job->settings = settings;
    job->estimatedCost = estimateCost(settings);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push_back(job);

        // Started by the first font that asks instead of at engine init, so a scene
        // that draws no text never spawns the thread at all.
        if (!m_isRunning) {
            m_isRunning = true;
            m_worker = std::thread(&FontLoader::workerMain, this);
        }
    }

    m_wakeup.notify_one();
    return job;
}

// Rough relative cost of a bake, used only to order the queue. A bitmap atlas is a
// glyph rasterisation; an mtsdf one colours the edges and solves a distance field per
// glyph, which is the order-of-magnitude difference the factor stands for. Both scale
// with the atlas area and the glyph count.
uint64_t FontLoader::estimateCost(const FontBakeSettings& settings)
{
    constexpr uint64_t MTSDF_COST_FACTOR = 20;
    constexpr uint64_t ASCII_GLYPHS = 96, LATIN1_GLYPHS = 224;

    uint64_t glyphCount =
        settings.charset == FontCharset::AsciiLatin1 ? LATIN1_GLYPHS : ASCII_GLYPHS;
    uint64_t area = (uint64_t)settings.emPixelSize * (uint64_t)settings.emPixelSize;
    uint64_t perGlyph = settings.atlasType == FontAtlasType::Mtsdf ? MTSDF_COST_FACTOR : 1;

    return glyphCount * area * perGlyph;
}

// Shortest job first: it minimises how long the average font spends as placeholder
// boxes, so a cheap bitmap atlas queued behind an mtsdf one does not wait seconds for
// work it could have finished in a fraction of the time. Nothing starves, because the
// queue is a startup burst rather than a live stream, and equal costs keep submission
// order so two identical fonts still arrive in the order they were asked for.
std::shared_ptr<FontLoadJob> FontLoader::takeCheapestJob()
{
    size_t cheapest = 0;
    for (size_t i = 1; i < m_queue.size(); i++) {
        if (m_queue[i]->estimatedCost < m_queue[cheapest]->estimatedCost) cheapest = i;
    }

    std::shared_ptr<FontLoadJob> job = m_queue[cheapest];
    m_queue.erase(m_queue.begin() + cheapest);
    return job;
}

// Below normal, so the scheduler always prefers the render thread and -- on a hybrid
// CPU -- parks this work on the efficiency cores. It only covers the serial half of a
// bake (glyph geometry, PNG encode, cache IO); msdf-atlas-gen's own threads start at
// normal priority whatever this one is, which is why the thread *count* is the lever
// that matters and this is the cheap extra.
void FontLoader::lowerWorkerPriority()
{
#if OS_NAME == OS_WINDOWS
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#endif
}

void FontLoader::workerMain()
{
    lowerWorkerPriority();

    while (true) {
        std::shared_ptr<FontLoadJob> job;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_wakeup.wait(lock, [this] { return !m_isRunning || !m_queue.empty(); });
            if (!m_isRunning) return;

            job = takeCheapestJob();
        }

        runJob(*job);
        job->isDone.store(true, std::memory_order_release);
    }
}

// Everything the old synchronous Font::load did except adopting the result, because
// that builds a GlTexture and GL belongs to the thread that owns the context.
void FontLoader::runJob(FontLoadJob& job)
{
    FontCacheKey key;
    if (!FontCache::buildKey(job.sourcePath, job.settings, key)) return;

    if (FontCache::load(key, job.settings, job.data)) {
        LOG_INFO("loaded the cached font atlas for {}", job.sourcePath);
        job.succeeded = true;
        return;
    }

    if (!FontBaker::bake(job.sourcePath, job.settings, job.data)) return;

    if (FontCache::store(key, job.settings, job.data)) FontCache::evictStale(key);
    job.succeeded = true;
}

}   // namespace Engine
