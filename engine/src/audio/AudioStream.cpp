#include "audio/AudioStream.hpp"
#include "core/logging/LoggerMacros.hpp"

namespace Engine {

AudioStream::AudioStream(const fs::path& filepath) : m_filepath(filepath)
{
    ma_decoder_config config = ma_decoder_config_init_default();
    config.format = ma_format_f32;
    config.channels = 2;         // Force Stereo
    config.sampleRate = 48000;   // Force 48kHz

    ma_decoder decoder;
    ma_result result = ma_decoder_init_file(filepath.string().c_str(), &config, &decoder);
    if (result != MA_SUCCESS) {
        LOG_ERROR("couldn't load audio stream from {}", filepath);
        return;
    }

    m_channels = decoder.outputChannels;
    m_sampleRate = decoder.outputSampleRate;

    ma_uint64 totalFrames = 0;
    ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames);
    m_totalFrameCount = totalFrames;

    ma_decoder_uninit(&decoder);
}

bool AudioStream::read(float* pOutput, ma_uint64 frameCount, ma_uint64 startingFrame)
{
    return false;
}

bool AudioStream::isStreamed() const { return true; }

bool AudioStream::initializeDecoder(ma_decoder* decoder)
{
    ma_decoder_config config = ma_decoder_config_init_default();
    config.format = ma_format_f32;
    config.channels = 2;         // Force Stereo
    config.sampleRate = 48000;   // Force 48kHz

    ma_result result = ma_decoder_init_file(m_filepath.string().c_str(), &config, decoder);
    if (result == MA_SUCCESS) return true;

    LOG_ERROR("couldn't load audio stream from {}", m_filepath);
    return false;
}

}   // namespace Engine
