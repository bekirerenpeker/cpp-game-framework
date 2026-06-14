#include "core/logging/ConsoleSink.hpp"
#include <iostream>
#include <format>

namespace Engine {

void ConsoleSink::Log(const LogMessage& message)
{
    std::cout << message.toString(true) << std::flush;
}

}   // namespace Engine
