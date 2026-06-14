#pragma once

#include <string>
#include <format>
#include "core/Time.hpp"

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
    std::string file, line, function;
    DateTime time;
    std::string message;

    std::string toString(bool useColor = true) const
    {
        std::string out;
        out.reserve(256);

        if (useColor) {
            std::string reset = "\x1b[0m";
            std::string gray = "\x1b[90m";
            std::string bold = "\x1b[1m";
            out += gray + "[" + time.toString() + "] " + getColorCode() + bold + "[" +
                   getLevelString() + "] " + reset;
        } else {
            out += "[" + time.toString() + "] [" + getLevelString() + "] ";
        }

        out += file + ":" + line + " (" + function + ") -> " + message + "\n";
        return out;
    }

  private:
    std::string getLevelString() const
    {
        switch (level) {
        case LogLevel::Info   : return "INFO";
        case LogLevel::Warning: return "WARNING";
        case LogLevel::Error  : return "ERROR";
        default               : return "UNKNOWN";
        }
    }
    std::string getColorCode() const
    {
        switch (level) {
        case LogLevel::Info   : return "\x1b[32m";   // Vibrant Green
        case LogLevel::Warning: return "\x1b[33m";   // Vibrant Yellow
        case LogLevel::Error  : return "\x1b[31m";   // Vibrant Red
        default               : return "\x1b[37m";                  // Default White
        }
    }
};

class ILogSink
{
  public:
    virtual ~ILogSink() = default;
    virtual void Log(const LogMessage& message) = 0;
};

}   // namespace Engine
