#pragma once

#include "ILogSink.hpp"

namespace Engine {

class ConsoleSink : public ILogSink
{
  public:
    void Log(const LogMessage& message) override;

  private:
    std::string getColorCode(LogLevel level) const;
};

}   // namespace Engine
