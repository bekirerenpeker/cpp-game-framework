#pragma once

#include "utils/Singleton.hpp"
#include <chrono>
#include <cstdint>
#include <string>

namespace Engine {

struct DateTime
{
    int year, month, day;
    int hour, minute, second;

    static std::string pad(int value) { return (value < 10 ? "0" : "") + std::to_string(value); }
    std::string toDateString() const
    {
        return std::to_string(year) + "-" + pad(month) + "-" + pad(day);
    }
    std::string toTimeString() const { return pad(hour) + ":" + pad(minute) + ":" + pad(second); }
    std::string toString() const { return toDateString() + " " + toTimeString(); }
};

class Time : public Singleton<Time>
{
    friend class Singleton<Time>;

  private:
    std::chrono::local_time<std::chrono::system_clock::duration> m_startupTime;
    double m_glfwTimeOffset;

    double m_deltaTime;
    double m_lastFrameTime;
    uint64_t m_frameCount;

    double m_fixedDeltaTime;
    double m_accumulator;
    int m_fixedTimeStepsInFrame;

  public:
    void update();

    float getDeltaTime() const;
    float getCurrTime() const;
    uint64_t getFrameCount() const { return m_frameCount; }

    DateTime getCurrentDateTime() const;

    void setFixedDeltaTime(float dt);
    float getFixedDeltaTime() const;
    int getFixedTimeStepsInFrame() const;

  private:
    Time();
    ~Time() = default;
};

}   // namespace Engine
