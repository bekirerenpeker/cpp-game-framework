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
  private:
    std::chrono::system_clock::time_point m_startupTime;
    double m_glfwTimeOffset;

    double m_deltaTime;
    double m_lastFrameTime;

  public:
    Time();
    ~Time() = default;

    void update();

    float deltaTime() const;
    float currTime() const;

    DateTime getCurrentDateTime() const;
    std::string currTimeStr() const;
};

}   // namespace Engine
