#pragma once

#include "audio/IAudioSource.hpp"
#include "core/file_management/FileManager.hpp"
#include "miniaudio/miniaudio.h"

namespace Engine {

class AudioStream : public IAudioSource
{
  private:
    ma_decoder m_decoder;

  public:
    AudioStream(fs::path filepath);
    ~AudioStream() override;

    bool read(float* pOutput, ma_uint64 frameCount, ma_uint64 startingFrame) override;
    bool isStreamed() const override;
};

}   // namespace Engine
