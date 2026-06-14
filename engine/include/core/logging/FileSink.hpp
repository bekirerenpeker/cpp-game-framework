#pragma once

#include "ILogSink.hpp"
#include "core/file_management/TextFile.hpp"

namespace Engine {

class FileSink : public ILogSink
{
  private:
    TextFile m_logFile;
    std::vector<std::string> m_buffer;
    const size_t m_flushThreshold = 50;

  public:
    FileSink(const fs::path& path);
    ~FileSink();

    void Log(const LogMessage& message) override;

  private:
    void flush();
};

}   // namespace Engine
