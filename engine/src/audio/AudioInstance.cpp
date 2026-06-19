#include "audio/AudioInstance.hpp"
#include "audio/AudioStream.hpp"
#include "audio/AudioManager.hpp"
#include <cassert>

namespace Engine {

AudioInstance::AudioInstance(
    IAudioSource* source, int busNodeIndex, PlaybackOptions options, ma_uint64 cursor
)
    : m_source(source), m_busNodeIndex(busNodeIndex), m_options(options), m_cursor(cursor)
{
    assert(m_source != nullptr && "source of an audio instance can not be nullptr");
    m_options.clampToValidRange();
    clampCursor();

    if (m_source->isStreamed()) {
        AudioStream* stream = static_cast<AudioStream*>(source);
        m_streamDecoder = new ma_decoder();
        bool success = stream->initializeDecoder(m_streamDecoder);
        if (!success) {
            delete m_streamDecoder;
            m_streamDecoder = nullptr;
        }
    }
}
AudioInstance::~AudioInstance()
{
    if (m_streamDecoder) {
        ma_decoder_uninit(m_streamDecoder);
        delete m_streamDecoder;
    }
}

AudioInstance::AudioInstance(AudioInstance&& other) noexcept
    : m_source(other.m_source),
      m_busNodeIndex(other.m_busNodeIndex),
      m_options(other.m_options),
      m_cursor(other.m_cursor),
      m_streamDecoder(other.m_streamDecoder)
{
    other.m_streamDecoder = nullptr;
}
AudioInstance& AudioInstance::operator=(AudioInstance&& other) noexcept
{
    if (this == &other) return *this;
    if (m_streamDecoder) {
        ma_decoder_uninit(m_streamDecoder);
        delete m_streamDecoder;
    }
    m_source = other.m_source;
    m_busNodeIndex = other.m_busNodeIndex;
    m_options = other.m_options;
    m_cursor = other.m_cursor;
    m_streamDecoder = other.m_streamDecoder;
    other.m_streamDecoder = nullptr;
    return *this;
}

bool AudioInstance::read(float* pOutput, ma_uint64 frameCount)
{
    float pitch = m_options.pitch;

    // --- FAST PATH (No Pitch Shifting) ---
    if (pitch == 1.0f) {
        bool result = readFrames(pOutput, frameCount);
        if (result) {
            m_cursor += frameCount;
            // Handle looping for the visual cursor
            if (m_options.loop && m_cursor >= m_source->getDurationFrames()) { m_cursor = 0; }
        }
        return result;
    }

    // --- PITCH SHIFTED PATH ---
    // 1. Calculate how many raw frames we need from the source
    double totalSourceFramesNeeded = m_subFrameOffset + (frameCount * pitch);
    ma_uint64 sourceFramesToRead = static_cast<ma_uint64>(std::ceil(totalSourceFramesNeeded)) + 1;

    constexpr int MAX_PITCH_FRAMES = 8192;
    ma_uint64 framesToRead = std::min(sourceFramesToRead, static_cast<ma_uint64>(MAX_PITCH_FRAMES));

    float sourceBuffer[MAX_PITCH_FRAMES * 2] = {0};

    // 2. Fetch the raw data using our stream/buffer agnostic helper
    bool fetchSuccess = readFrames(sourceBuffer, framesToRead);
    if (!fetchSuccess) return false;

    // 3. Interpolate the raw data and update the cursors
    return processFrames(pOutput, frameCount, sourceBuffer, framesToRead);
}
bool AudioInstance::readFrames(float* pOutput, ma_uint64 framesToRead)
{
    if (m_source->isStreamed()) {
        if (!m_streamDecoder) return false;

        ma_uint64 totalFramesRead = 0;

        // We use a loop here to handle seamless wrapping if m_options.loop is true
        while (totalFramesRead < framesToRead) {
            ma_uint64 framesToRequest = framesToRead - totalFramesRead;
            ma_uint64 framesReadThisIteration = 0;

            float* writePtr = pOutput + (totalFramesRead * 2);   // 2 for stereo

            ma_decoder_read_pcm_frames(
                m_streamDecoder, writePtr, framesToRequest, &framesReadThisIteration
            );
            totalFramesRead += framesReadThisIteration;

            // If we didn't get all the frames, we hit the end of the file
            if (framesReadThisIteration < framesToRequest) {
                if (m_options.loop) {
                    // Instantly wrap the decoder to 0 and let the loop fetch the rest
                    ma_decoder_seek_to_pcm_frame(m_streamDecoder, 0);
                } else {
                    break;   // Not looping, end of file reached
                }
            }
        }
        return totalFramesRead > 0;
    } else {
        return m_source->read(pOutput, framesToRead, m_cursor);
    }
}
bool AudioInstance::processFrames(
    float* pOutput, ma_uint64 frameCount, float* sourceBuffer, ma_uint64 framesRead
)
{
    float pitch = m_options.pitch;

    // 1. Interpolate using the stable m_subFrameOffset
    for (ma_uint64 i = 0; i < frameCount; ++i) {
        double virtualPos = m_subFrameOffset + (i * pitch);
        int index0 = static_cast<int>(virtualPos);
        int index1 = index0 + 1;
        double frac = virtualPos - index0;

        if (index1 >= framesRead) break;

        // Left & Right channel interpolation
        float left0 = sourceBuffer[index0 * 2 + 0];
        float left1 = sourceBuffer[index1 * 2 + 0];
        pOutput[i * 2 + 0] = left0 + frac * (left1 - left0);

        float right0 = sourceBuffer[index0 * 2 + 1];
        float right1 = sourceBuffer[index1 * 2 + 1];
        pOutput[i * 2 + 1] = right0 + frac * (right1 - right0);
    }

    // 2. CRITICAL STEP: Update the cursor positions safely
    double totalAdvance = m_subFrameOffset + (frameCount * pitch);
    ma_uint64 integerFramesWalked = static_cast<ma_uint64>(std::floor(totalAdvance));

    m_subFrameOffset = static_cast<float>(totalAdvance - integerFramesWalked);
    m_cursor += integerFramesWalked;

    // 3. Loop logic for the main visual cursor
    if (m_options.loop && m_cursor >= m_source->getDurationFrames()) {
        m_cursor = 0;
        m_subFrameOffset = 0.0f;   // Reset fraction on loop
    }

    return true;
}

const IAudioSource* AudioInstance::getSource() const { return m_source; }

void AudioInstance::clampCursor()
{
    if (m_cursor > m_source->getDurationFrames()) m_cursor = m_source->getDurationFrames() - 1;
}
void AudioInstance::updateStreamCursor()
{
    if (!m_streamDecoder) return;
    ma_result result = ma_decoder_seek_to_pcm_frame(m_streamDecoder, m_cursor);
    if (result == MA_SUCCESS) m_subFrameOffset = 0.0f;
    else LOG_ERROR("couldn't set the cursor for streamed audio instance");
}
ma_uint64 AudioInstance::getCursorFrames() const { return m_cursor; }

float AudioInstance::getCursorSeconds() const
{
    return static_cast<double>(m_cursor) / static_cast<double>(m_source->getSampleRate());
}
void AudioInstance::setCursorFrames(ma_uint64 cursor)
{
    m_cursor = cursor;
    clampCursor();
    updateStreamCursor();
}
void AudioInstance::setCursorSeconds(float cursor)
{
    m_cursor = static_cast<ma_uint64>(cursor * static_cast<double>(m_source->getSampleRate()));
    clampCursor();
    updateStreamCursor();
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
