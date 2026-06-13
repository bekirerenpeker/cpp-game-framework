#include "core/logging/Logger.hpp"
#include "core/logging/ConsoleSink.hpp"

namespace Engine {

Logger::Logger()
{
    m_sinks.push_back(new ConsoleSink());
    setUseAsync(true);
}

Logger::~Logger()
{
    stopWorker();
    clearSinks();
}

void Logger::setMinLogLevel(LogLevel level) { m_minLogLevel = level; }
LogLevel Logger::getMinLogLevel() const { return m_minLogLevel; }

void Logger::setUseAsync(bool useAsync)
{
    if (m_useAsync == useAsync) return;
    m_useAsync = useAsync;

    if (m_useAsync) startWorker();
    else stopWorker();
}
bool Logger::getUseAsync() const { return m_useAsync; }

void Logger::clearSinks()
{
    std::lock_guard<std::mutex> lock(m_sinkMutex);
    for (ILogSink* sink : m_sinks) delete sink;
    m_sinks.clear();
}
void Logger::writeToSinks(const LogMessage& message)
{
    std::lock_guard<std::mutex> lock(m_sinkMutex);
    for (ILogSink* sink : m_sinks) sink->Log(message);
}

void Logger::startWorker()
{
    if (m_threadRunning) return;
    m_threadRunning = true;
    m_loggerThread = std::thread(&Logger::workerLoop, this);
}
void Logger::stopWorker()
{
    if (!m_threadRunning) return;
    m_threadRunning = false;
    m_queueCondition.notify_one();
    if (m_loggerThread.joinable()) { m_loggerThread.join(); }
}
void Logger::workerLoop()
{
    while (m_threadRunning || !m_logQueue.empty()) {
        std::queue<LogMessage> localQueue;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);

            // Sleep until new logs arrive or stopWorker() is called
            m_queueCondition.wait(lock, [this]() {
                return !m_logQueue.empty() || !m_threadRunning;
            });

            // Drain entire queue into local variable to minimize hold time on the mutex
            while (!m_logQueue.empty()) {
                localQueue.push(std::move(m_logQueue.front()));
                m_logQueue.pop();
            }
        }

        // Process logs entirely off the main game execution thread
        while (!localQueue.empty()) {
            writeToSinks(localQueue.front());
            localQueue.pop();
        }
    }
}

void Logger::logGlfwMessage(int error, const char* description)
{
    Logger::get().Log(LogLevel::Error, "", "", "", "glfw error: {}: {}", error, description);
}

}   // namespace Engine
