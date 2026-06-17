#pragma once

#include "utils/math/MathFuncs.hpp"

namespace Engine {

struct PlaybackOptions
{
    float volume = 1.0f;   // 0.0 (silent) to 1.0 (full)
    float pitch = 1.0f;    // 1.0 is default, 0.5 is half, 2.0 is double
    float pan = 0.0f;      // -1.0 (full left) to 1.0 (full right), 0.0 center
    bool loop = false;

    PlaybackOptions() = default;
    PlaybackOptions(float volume, float pitch, float pan, bool loop)
        : volume(volume), pitch(pitch), pan(pan), loop(loop)
    {
    }

    PlaybackOptions combined(const PlaybackOptions& parent) const
    {
        PlaybackOptions combined;
        combined.volume = this->volume * parent.volume;
        combined.pitch = this->pitch * parent.pitch;
        combined.pan = Math::clamp(this->pan + parent.pan, -1.0f, 1.0f);
        combined.loop = this->loop;
        return combined;
    }
    void combine(const PlaybackOptions& parent) { *this = combined(parent); }

    void clampToValidRange()
    {
        volume = Math::clamp(volume, 0.f, 1.f);
        pitch = Math::clamp(volume, 0.f, 10.f);
        pan = Math::clamp(volume, -1.f, 1.f);
    }
};

}   // namespace Engine
