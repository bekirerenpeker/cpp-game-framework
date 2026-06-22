#pragma once

#include "ILogSink.hpp"

namespace Engine {

class ConsoleSink : public ILogSink
{
  public:
    ConsoleSink(LogLevel minLogLevel = LogLevel::Info) : ILogSink(minLogLevel) {}
    void log(const LogMessage& message) override;
};

}   // namespace Engine
