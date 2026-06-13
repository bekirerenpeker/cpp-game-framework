#include "core/Time.hpp"
#include "context/GlfwContext.hpp"
#include "GLFW/glfw3.h"
#include <format>

namespace Engine {

Time::Time()
{
    GlfwContext::init();

    m_startupTime = std::chrono::system_clock::now();
    m_glfwTimeOffset = glfwGetTime();

    m_deltaTime = 0.0;
    m_lastFrameTime = m_glfwTimeOffset;
}

float Time::deltaTime() const { return m_deltaTime; }

float Time::currTime() const { return glfwGetTime() - m_glfwTimeOffset; }

void Time::update()
{
    float currentFrameTime = static_cast<float>(glfwGetTime());
    m_deltaTime = currentFrameTime - m_lastFrameTime;
    m_lastFrameTime = currentFrameTime;
}

DateTime Time::getCurrentDateTime() const
{
    auto elapsedDuration = std::chrono::duration_cast<std::chrono::system_clock::duration>(
        std::chrono::duration<double>(currTime())
    );
    std::chrono::system_clock::time_point currentTime = m_startupTime + elapsedDuration;

    auto sysSeconds = std::chrono::floor<std::chrono::seconds>(currentTime);
    std::chrono::sys_days sysDays = std::chrono::floor<std::chrono::days>(sysSeconds);
    std::chrono::year_month_day ymd {sysDays};
    std::chrono::hh_mm_ss timeOfDay {sysSeconds - sysDays};

    return DateTime {
        static_cast<int>(ymd.year()),
        static_cast<int>(static_cast<unsigned>(ymd.month())),
        static_cast<int>(static_cast<unsigned>(ymd.day())),
        static_cast<int>(timeOfDay.hours().count()),
        static_cast<int>(timeOfDay.minutes().count()),
        static_cast<int>(timeOfDay.seconds().count())
    };
}

std::string Time::currTimeStr() const
{
    DateTime currentDateTime = getCurrentDateTime();
    return std::format(
        "{:02}:{:02}:{:02}", currentDateTime.hour, currentDateTime.minute, currentDateTime.second
    );
}

}   // namespace Engine
