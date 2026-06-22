#include "core/logging/ConsoleSink.hpp"
#include <iostream>
#include <format>

namespace Engine {

void ConsoleSink::log(const LogMessage& message)
{
    if ((int)message.level < (int)m_minLogLevel) return;
    std::cout << message.toString(true) << std::flush;
}

}   // namespace Engine
