#include "audio/IAudioSource.hpp"

namespace Engine {

ma_uint64 IAudioSource::getDurationFrames() const { return m_totalFrameCount; }
float IAudioSource::getDurationSeconds() const
{
    return static_cast<double>(m_totalFrameCount) / static_cast<double>(m_sampleRate);
}

ma_uint32 IAudioSource::getSampleRate() const { return m_sampleRate; }
ma_uint32 IAudioSource::getChannelCount() const { return m_channels; }

}   // namespace Engine
