#pragma once

#include "audio/IAudioSource.hpp"
#include "core/file_management/FileManager.hpp"
#include "miniaudio/miniaudio.h"

namespace Engine {

class AudioStream : public IAudioSource
{
  private:
    fs::path m_filepath;

  public:
    AudioStream(const fs::path& filepath);
    ~AudioStream() override = default;

    bool read(float* pOutput, ma_uint64 frameCount, ma_uint64 startingFrame) override;
    bool isStreamed() const override;

    bool initializeDecoder(ma_decoder* decoder);
};

}   // namespace Engine
