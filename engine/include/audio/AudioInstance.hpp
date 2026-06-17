#pragma once

#include "audio/PlaybackOptions.hpp"
#include "audio/IAudioSource.hpp"
#include "miniaudio/miniaudio.h"

namespace Engine {

struct AudioInstance
{
  private:
    IAudioSource* m_source;
    PlaybackOptions m_options;
    ma_uint64 m_cursor;
    bool m_isPaused;

  public:
    AudioInstance(IAudioSource* source, PlaybackOptions options = {}, ma_uint64 cursor = 0);
    ~AudioInstance() = default;

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
