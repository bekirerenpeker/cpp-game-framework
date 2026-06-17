#include "audio/AudioInstance.hpp"
#include <cassert>

namespace Engine {

AudioInstance::AudioInstance(IAudioSource* source, PlaybackOptions options, ma_uint64 cursor)
    : m_source(source), m_options(options), m_cursor(cursor)
{
    assert(m_source != nullptr && "source of an audio instance can not be nullptr");
    m_options.clampToValidRange();
    clampCursor();
}

bool AudioInstance::read(float* pOutput, ma_uint64 frameCount)
{
    float pitch = m_options.pitch;

    if (pitch == 1.0) {
        bool result = m_source->read(pOutput, frameCount, m_cursor);
        m_cursor += frameCount;
        if (m_options.loop && m_cursor >= m_source->getDurationFrames()) {
            m_cursor = 0;
            return true;
        }
        return result;
    }

    ma_uint64 startFrame = m_cursor;
    double totalSourceFramesNeeded = m_subFrameOffset + (frameCount * pitch);
    ma_uint64 sourceFramesToRead = static_cast<ma_uint64>(std::ceil(totalSourceFramesNeeded)) + 1;

    constexpr int MAX_PITCH_FRAMES = 8192;
    ma_uint64 framesToRead = std::min(sourceFramesToRead, static_cast<ma_uint64>(MAX_PITCH_FRAMES));

    float sourceBuffer[MAX_PITCH_FRAMES * 2] = {0};
    bool result = m_source->read(sourceBuffer, framesToRead, startFrame);
    if (!result) return false;

    // 2. Interpolate using the stable m_subFrameOffset
    for (ma_uint64 i = 0; i < frameCount; ++i) {
        double virtualPos = m_subFrameOffset + (i * pitch);
        int index0 = static_cast<int>(virtualPos);
        int index1 = index0 + 1;
        double frac = virtualPos - index0;

        if (index1 >= framesToRead) break;

        // Left & Right channel interpolation
        float left0 = sourceBuffer[index0 * 2 + 0];
        float left1 = sourceBuffer[index1 * 2 + 0];
        pOutput[i * 2 + 0] = left0 + frac * (left1 - left0);

        float right0 = sourceBuffer[index0 * 2 + 1];
        float right1 = sourceBuffer[index1 * 2 + 1];
        pOutput[i * 2 + 1] = right0 + frac * (right1 - right0);
    }

    // 3. CRITICAL STEP: Update the cursor positions safely
    // Add the total playback jump to our accumulator
    double totalAdvance = m_subFrameOffset + (frameCount * pitch);

    // Extract the whole integer frames walked during this block
    ma_uint64 integerFramesWalked = static_cast<ma_uint64>(std::floor(totalAdvance));

    // Save ONLY the remainder back into the sub-frame offset (keeps it under 1.0)
    m_subFrameOffset = static_cast<float>(totalAdvance - integerFramesWalked);

    // Step the main integer cursor forward cleanly
    m_cursor += integerFramesWalked;

    // 4. Loop logic
    if (m_options.loop && m_cursor >= m_source->getDurationFrames()) {
        m_cursor = 0;
        m_subFrameOffset = 0.0f;   // Reset fraction on loop
        return true;
    }

    return true;
}

const IAudioSource* AudioInstance::getSource() const { return m_source; }

ma_uint64 AudioInstance::getCursorFrames() const { return m_cursor; }
float AudioInstance::getCursorSeconds() const
{
    return static_cast<double>(m_cursor) / static_cast<double>(m_source->getSampleRate());
}

void AudioInstance::clampCursor()
{
    if (m_cursor > m_source->getDurationFrames()) m_cursor = m_source->getDurationFrames() - 1;
}
void AudioInstance::setCursorFrames(ma_uint64 cursor)
{
    m_cursor = cursor;
    clampCursor();
}
void AudioInstance::setCursorSeconds(float cursor)
{
    m_cursor = static_cast<ma_uint64>(cursor * static_cast<double>(m_source->getSampleRate()));
    clampCursor();
}

PlaybackOptions AudioInstance::getOptions() const { return m_options; }
bool AudioInstance::isPaused() const { return m_isPaused; }

void AudioInstance::setOptions(const PlaybackOptions& options)
{
    m_options = options;
    m_options.clampToValidRange();
}
void AudioInstance::setIsPaused(bool isPaused) { m_isPaused = isPaused; }

}   // namespace Engine
