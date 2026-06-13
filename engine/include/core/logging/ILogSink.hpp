#pragma once

#include <string>

namespace Engine {

enum class LogLevel
{
    Info,
    Warning,
    Error
};

struct LogMessage
{
    LogLevel level;
    std::string file;
    std::string line;
    std::string function;
    std::string time;
    std::string message;
};

class ILogSink
{
  public:
    virtual ~ILogSink() = default;
    virtual void Log(const LogMessage& message) = 0;

  protected:
    std::string getLevelString(LogLevel level)
    {
        switch (level) {
        case LogLevel::Info   : return "INFO";
        case LogLevel::Warning: return "WARNING";
        case LogLevel::Error  : return "ERROR";
        default               : return "UNKNOWN";
        }
    }
};

}   // namespace Engine
