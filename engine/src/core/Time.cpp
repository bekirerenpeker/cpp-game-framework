#include "core/Time.hpp"
#include "context/GlfwContext.hpp"
#include "GLFW/glfw3.h"
#include <format>

namespace Engine {

Time::Time()
{
    GlfwContext::init();

    auto utcNow = std::chrono::system_clock::now();
    m_startupTime = std::chrono::current_zone()->to_local(utcNow);
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
    auto currentTime = m_startupTime + elapsedDuration;

    auto localSeconds = std::chrono::floor<std::chrono::seconds>(currentTime);
    std::chrono::local_days localDays = std::chrono::floor<std::chrono::days>(localSeconds);

    std::chrono::year_month_day ymd {localDays};
    std::chrono::hh_mm_ss timeOfDay {localSeconds - localDays};

    return DateTime {
        static_cast<int>(ymd.year()),
        static_cast<int>(static_cast<unsigned>(ymd.month())),
        static_cast<int>(static_cast<unsigned>(ymd.day())),
        static_cast<int>(timeOfDay.hours().count()),
        static_cast<int>(timeOfDay.minutes().count()),
        static_cast<int>(timeOfDay.seconds().count())
    };
}

}   // namespace Engine
