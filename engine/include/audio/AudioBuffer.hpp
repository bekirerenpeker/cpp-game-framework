#pragma once

#include "audio/IAudioSource.hpp"
#include "core/file_management/FileManager.hpp"

namespace Engine {

class AudioBuffer : public IAudioSource
{
  private:
    float* m_pcmData = nullptr;

  public:
    AudioBuffer(const fs::path& filepath);
    ~AudioBuffer() override;

    bool read(float* pOutput, ma_uint64 frameCount, ma_uint64 startingFrame) override;
    bool isStreamed() const override;
};

}   // namespace Engine
