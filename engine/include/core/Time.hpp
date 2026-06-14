#pragma once

#include "utils/Singleton.hpp"
#include <chrono>
#include <string>

namespace Engine {

struct DateTime
{
    int year, month, day;
    int hour, minute, second;
};

class Time : public Singleton<Time>
{
    friend class Singleton<Time>;

  private:
    std::chrono::local_time<std::chrono::system_clock::duration> m_startupTime;
    double m_glfwTimeOffset;

    double m_deltaTime;
    double m_lastFrameTime;

  public:
    void update();

    float deltaTime() const;
    float currTime() const;

    DateTime getCurrentDateTime() const;
    std::string currTimeStr() const;

  private:
    Time();
    ~Time() = default;
};

}   // namespace Engine
