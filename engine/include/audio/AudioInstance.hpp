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
    int m_busNodeIndex;   // int is enough since there wont be many bus nodes
    bool m_isPaused = false;
    float m_fade, m_fadeSpeed;

    ma_uint64 m_cursor;
    float m_subFrameOffset = 0.0f;

    ma_decoder* m_streamDecoder = nullptr;

  public:
    AudioInstance(
        IAudioSource* source, int busNodeIndex, PlaybackOptions options = {}, ma_uint64 cursor = 0
    );
    ~AudioInstance();

    AudioInstance(AudioInstance&& other) noexcept;
    AudioInstance& operator=(AudioInstance&& other) noexcept;

    bool read(float* pOutput, ma_uint64 frameCount);
    const IAudioSource* getSource() const;

    void fadeIn(float duration = 1.f);
    void fadeOut(float duration = 1.f);
    bool isFading();

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
    void updateStreamCursor();

    bool readFrames(float* pOutput, ma_uint64 framesToRead);
    bool
    processFrames(float* pOutput, ma_uint64 frameCount, float* sourceBuffer, ma_uint64 framesRead);
};

}   // namespace Engine
