#include "audio/AudioBuffer.hpp"
#include "miniaudio/miniaudio.h"
#include "core/logging/LoggerMacros.hpp"
#include <algorithm>
#include <cstring>

namespace Engine {

AudioBuffer::AudioBuffer(const fs::path& filepath)
{
    ma_decoder_config config = ma_decoder_config_init_default();
    config.format = ma_format_f32;
    config.channels = 2;         // Force Stereo
    config.sampleRate = 48000;   // Force 48kHz

    ma_decoder decoder;
    ma_result result = ma_decoder_init_file(filepath.string().c_str(), &config, &decoder);

    if (result != MA_SUCCESS) {
        LOG_ERROR("couldn't load audio buffer from {}", filepath);
        return;
    }

    m_channels = decoder.outputChannels;
    m_sampleRate = decoder.outputSampleRate;

    void* pData = nullptr;
    ma_uint64 frameCount = 0;
    result = ma_decode_file(filepath.string().c_str(), &config, &frameCount, &pData);

    if (result != MA_SUCCESS) {
        LOG_ERROR("couldn't load audio buffer from {}", filepath);
        return;
    }

    m_pcmData = static_cast<float*>(pData);
    m_totalFrameCount = frameCount;
    ma_decoder_uninit(&decoder);
}

AudioBuffer::~AudioBuffer()
{
    if (m_pcmData) ma_free(m_pcmData, nullptr);
};

bool AudioBuffer::read(float* pOutput, ma_uint64 frameCount, ma_uint64 startingFrame)
{
    if (startingFrame >= m_totalFrameCount) return false;

    ma_uint64 framesRemaining = m_totalFrameCount - startingFrame;
    ma_uint64 framesToRead = std::min(frameCount, framesRemaining);

    const float* pSource = m_pcmData + (startingFrame * 2);   // * 2 for Stereo
    std::memcpy(pOutput, pSource, framesToRead * 2 * sizeof(float));

    return true;
}

bool AudioBuffer::isStreamed() const { return false; }

}   // namespace Engine
