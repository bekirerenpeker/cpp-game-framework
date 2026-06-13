#include "core/logging/FileSink.hpp"

namespace Engine {

FileSink::FileSink(const std::filesystem::path& filePath) : m_filePath(filePath) {}

void FileSink::Log(const LogMessage& message) {}

}   // namespace Engine
