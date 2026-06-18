#pragma once

#include "audio/PlaybackOptions.hpp"
#include "audio/IAudioSource.hpp"
#include "utils/IdIndexedVector.hpp"
#include "miniaudio/miniaudio.h"

namespace Engine {

struct AudioInstance : public IHasId
{
    friend class AudioManager;

  private:
    IAudioSource* m_source;
    PlaybackOptions m_options;
    bool m_isPaused = false;

    ma_uint64 m_cursor;
    float m_subFrameOffset = 0.0f;

    ma_decoder* m_streamDecoder = nullptr;

  public:
    AudioInstance(IAudioSource* source, PlaybackOptions options = {}, ma_uint64 cursor = 0);
    ~AudioInstance();

    AudioInstance(AudioInstance&& other) noexcept;
    AudioInstance& operator=(AudioInstance&& other) noexcept;

    bool read(float* pOutput, ma_uint64 frameCount);
    const IAudioSource* getSource() const;

  private:
    // these are not thread safe change from AudioManager
    ma_uint64 getCursorFrames() const;
    float getCursorSeconds() const;
    void setCursorFrames(ma_uint64 cursor);
    void setCursorSeconds(float cursor);
    PlaybackOptions getOptions() const;
    bool isPaused() const;
    void setOptions(const PlaybackOptions& options);
    void setIsPaused(bool isPaused);

    void clampCursor();
    void updateStreamCursor();

    bool readFrames(float* pOutput, ma_uint64 framesToRead);
    bool
    processFrames(float* pOutput, ma_uint64 frameCount, float* sourceBuffer, ma_uint64 framesRead);
};

}   // namespace Engine
