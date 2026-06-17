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
    return m_source->read(pOutput, frameCount, m_cursor);
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
