#pragma once

#include "audio/PlaybackOptions.hpp"
#include "audio/IAudioSource.hpp"
#include "utils/IdIndexedVector.hpp"
#include "miniaudio/miniaudio.h"

namespace Engine {

struct AudioInstance : public IHasId
{
  private:
    IAudioSource* m_source;
    PlaybackOptions m_options;
    ma_uint64 m_cursor;
    bool m_isPaused;

  public:
    AudioInstance(IAudioSource* source, PlaybackOptions options = {}, ma_uint64 cursor = 0);
    ~AudioInstance() = default;

    bool read(float* pOutput, ma_uint64 frameCount);
    const IAudioSource* getSource() const;

    ma_uint64 getCursorFrames() const;
    float getCursorSeconds() const;

    void setCursorFrames(ma_uint64 cursor);
    void setCursorSeconds(float cursor);

    PlaybackOptions getOptions() const;
    bool isPaused() const;

    void setOptions(const PlaybackOptions& options);
    void setIsPaused(bool isPaused);

  private:
    void clampCursor();
};

}   // namespace Engine
