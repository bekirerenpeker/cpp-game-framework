#pragma once

#include "ILogSink.hpp"
#include <filesystem>

namespace Engine {

class FileSink : public ILogSink
{
  private:
    std::filesystem::path m_filePath;

  public:
    FileSink(const std::filesystem::path& filePath);

    void Log(const LogMessage& message) override;
};

}   // namespace Engine
