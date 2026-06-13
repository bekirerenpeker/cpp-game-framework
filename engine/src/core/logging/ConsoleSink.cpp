#include "core/logging/ConsoleSink.hpp"
#include <iostream>
#include <format>

namespace Engine {

std::string ConsoleSink::getColorCode(LogLevel level) const
{
    switch (level) {
    case LogLevel::Info   : return "\x1b[32m";   // Vibrant Green
    case LogLevel::Warning: return "\x1b[33m";   // Vibrant Yellow
    case LogLevel::Error  : return "\x1b[31m";   // Vibrant Red
    default               : return "\x1b[37m";                  // Default White
    }
}

void ConsoleSink::Log(const LogMessage& message)
{
    // Get our color tokens
    std::string reset = "\x1b[0m";
    std::string gray = "\x1b[90m";
    std::string bold = "\x1b[1m";
    std::string color = getColorCode(message.level);

    // Format everything into a single block of text and print it once
    std::cout << std::format(
                     "{}[{}] {}{}[{}] {}{}:{} ({}) -> {}{}\n", gray, message.time, color, bold,
                     getLevelString(message.level), reset, message.file, message.line,
                     message.function, message.message, reset
                 )
              << std::flush;
}

}   // namespace Engine
