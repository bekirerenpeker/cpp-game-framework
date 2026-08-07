#pragma once

#include "graphics/text/Font.hpp"
#include "utils/Singleton.hpp"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace Engine {

// The handoff between a Font and the worker. Both ends hold it, so a Font destroyed
// while its atlas is still baking leaves the job alive to finish and throw its result
// away, rather than the worker writing into freed memory. isDone is the only field
// touched by both threads: the worker fills everything else first and releases it
// last, so a reader that sees the flag sees a complete result.
struct FontLoadJob
{
    fs::path sourcePath;
    FontBakeSettings settings;
    FontData data;
    uint64_t estimatedCost = 0;
    std::atomic<bool> isDone {false};
    bool succeeded = false;
};

class FontLoader : public Singleton<FontLoader>
{
    friend class Singleton<FontLoader>;

  private:
    std::vector<std::shared_ptr<FontLoadJob>> m_queue;
    std::thread m_worker;
    std::mutex m_mutex;
    std::condition_variable m_wakeup;
    bool m_isRunning = false;

    Font* m_defaultFont = nullptr;
    bool m_defaultRequested = false;

  public:
    std::shared_ptr<FontLoadJob>
    submit(const fs::path& sourcePath, const FontBakeSettings& settings);

    // A bitmap atlas of whatever face the OS has, baked on demand and owned here. It is
    // queued ahead of the first font anyone else asks for, because it is what every
    // other font borrows while its own atlas is still baking.
    const Font* getDefaultFont();

    // What a text call actually draws with: the default when the caller passed nothing,
    // and the default again while the caller's own font is still baking -- so loading
    // text reads in a real face instead of placeholder boxes. Only when the default is
    // not ready either does the request fall back to itself, and the boxes are all
    // there is.
    const Font* resolve(const Font* requested);

  private:
    FontLoader() = default;
    ~FontLoader();

    void ensureDefaultFont();
    std::shared_ptr<FontLoadJob> takeCheapestJob();

    void workerMain();
    static void lowerWorkerPriority();
    static void runJob(FontLoadJob& job);
    static uint64_t estimateCost(const FontBakeSettings& settings);
};

}   // namespace Engine
