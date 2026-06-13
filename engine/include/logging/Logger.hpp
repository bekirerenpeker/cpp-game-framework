#pragma once

#include "utils/Singleton.hpp"
#include "logging/ILogSink.hpp"
#include <format>
#include <string>
#include <string_view>
#include <vector>
#include <queue>
#include <mutex>

namespace Engine {

class Logger : public Singleton<Logger>
{
  private:
    std::vector<ILogSink*> m_sinks;
    std::queue<LogMessage> m_logQueue;
    LogLevel m_minLogLevel = LogLevel::Info;

    std::mutex m_sinkMutex, m_queueMutex;
    std::condition_variable m_queueCondition;
    std::thread m_loggerThread;
    std::atomic<bool> m_useAsync = false, m_threadRunning = false;

  public:
    Logger();
    ~Logger();

    void setMinLogLevel(LogLevel level);
    LogLevel getMinLogLevel() const;

    void setUseAsync(bool useAsync);
    bool getUseAsync() const;

    template<typename T, typename... Args> void addSink(Args&&... args);
    void clearSinks();
    void writeToSinks(const LogMessage& message);

    template<typename... Args>
    void
    Log(LogLevel level, const std::string& file, const std::string& line,
        const std::string& function, const std::string& time, const std::string_view& message,
        Args&&... args);

  private:
    void workerLoop();
    void startWorker();
    void stopWorker();
};

template<typename T, typename... Args> void Logger::addSink(Args&&... args)
{
    m_sinks.push_back(new T(std::forward<Args>(args)...));
}

template<typename... Args>
void Logger::Log(
    LogLevel level, const std::string& file, const std::string& line, const std::string& function,
    const std::string& time, const std::string_view& message, Args&&... args
)
{
    if (level < m_minLogLevel) return;
    std::string formattedMessage = std::vformat(message, std::make_format_args(args...));
    LogMessage logMessage = {level, file, line, function, time, formattedMessage};

    if (m_useAsync) {
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_logQueue.push(logMessage);
        }
        m_queueCondition.notify_one();
    } else {
        writeToSinks(logMessage);
    }
}

}   // namespace Engine
