#include "core/Time.hpp"
#include "context/GlfwContext.hpp"
#include "GLFW/glfw3.h"

#define MAX_FRAME_TIME  0.25
#define MAX_FIXED_STEPS 5

namespace Engine {

Time::Time()
{
    GlfwContext::init();

    auto utcNow = std::chrono::system_clock::now();
    m_startupTime = std::chrono::current_zone()->to_local(utcNow);
    m_glfwTimeOffset = glfwGetTime();

    m_deltaTime = 0.0;
    m_lastFrameTime = m_glfwTimeOffset;
    m_frameCount = 0;

    m_fixedDeltaTime = 1 / 60.f;   // 60 fps
    m_accumulator = 0.0;
    m_fixedTimeStepsInFrame = 0;
}

float Time::getDeltaTime() const { return m_deltaTime; }

float Time::getCurrTime() const { return glfwGetTime() - m_glfwTimeOffset; }

void Time::update()
{
    double currentFrameTime = glfwGetTime();

    m_deltaTime = currentFrameTime - m_lastFrameTime;
    if (m_deltaTime > MAX_FRAME_TIME) m_deltaTime = MAX_FRAME_TIME;

    m_lastFrameTime = currentFrameTime;
    m_frameCount++;

    m_accumulator += m_deltaTime;
    m_fixedTimeStepsInFrame = static_cast<int>(m_accumulator / m_fixedDeltaTime);
    m_accumulator -= m_fixedTimeStepsInFrame * m_fixedDeltaTime;

    if (m_fixedTimeStepsInFrame > MAX_FIXED_STEPS) {
        m_fixedTimeStepsInFrame = MAX_FIXED_STEPS;
        m_accumulator = 0.0;
    }
}

DateTime Time::getCurrentDateTime() const
{
    auto elapsedDuration = std::chrono::duration_cast<std::chrono::system_clock::duration>(
        std::chrono::duration<double>(getCurrTime())
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

void Time::setFixedDeltaTime(float dt)
{
    if (dt > 0) m_fixedDeltaTime = dt;
}
float Time::getFixedDeltaTime() const { return m_fixedDeltaTime; }
int Time::getFixedTimeStepsInFrame() const { return m_fixedTimeStepsInFrame; }

}   // namespace Engine
