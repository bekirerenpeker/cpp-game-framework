#pragma once

#include "miniaudio/miniaudio.h"
#include "core/resource_management/IResource.hpp"

namespace Engine {

class IAudioSource : public IResource
{
  protected:
    ma_uint64 m_totalFrameCount;
    ma_uint32 m_sampleRate;
    ma_uint32 m_channels;

  public:
    IAudioSource() = default;
    virtual ~IAudioSource() = default;

    virtual bool read(float* pOutput, ma_uint64 frameCount, ma_uint64 startingFrame) = 0;
    virtual bool isStreamed() const = 0;

    ma_uint32 getSampleRate() const;
    ma_uint32 getChannelCount() const;

    ma_uint64 getDurationFrames() const;
    float getDurationSeconds() const;
};

}   // namespace Engine
