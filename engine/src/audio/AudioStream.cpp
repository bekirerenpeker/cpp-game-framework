#include "audio/AudioStream.hpp"
#include "core/logging/LoggerMacros.hpp"

namespace Engine {

AudioStream::AudioStream(const fs::path& filepath)
{
    ma_decoder_config config = ma_decoder_config_init_default();
    config.format = ma_format_f32;
    config.channels = 2;         // Force Stereo
    config.sampleRate = 48000;   // Force 48kHz

    ma_result result = ma_decoder_init_file(filepath.string().c_str(), &config, &m_decoder);
    if (result != MA_SUCCESS) {
        LOG_ERROR("couldn't load audio stream from {}", filepath);
        return;
    }

    m_channels = m_decoder.outputChannels;
    m_sampleRate = m_decoder.outputSampleRate;

    ma_uint64 totalFrames = 0;
    ma_decoder_get_length_in_pcm_frames(&m_decoder, &totalFrames);
    m_totalFrameCount = totalFrames;
}

AudioStream::~AudioStream() { ma_decoder_uninit(&m_decoder); }

bool AudioStream::read(float* pOutput, ma_uint64 frameCount, ma_uint64 startingFrame)
{
    ma_decoder_seek_to_pcm_frame(&m_decoder, startingFrame);

    ma_uint64 framesRead = 0;
    ma_decoder_read_pcm_frames(&m_decoder, pOutput, frameCount, &framesRead);

    return framesRead > 0;
}

bool AudioStream::isStreamed() const { return true; }

}   // namespace Engine
