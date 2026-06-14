#include "core/logging/FileSink.hpp"

namespace Engine {

FileSink::FileSink(const std::filesystem::path& filePath) : m_logFile("")
{
    if (!FileManager::get().doesPathExist(filePath)) FileManager::get().createFile(filePath);
    m_logFile = TextFile(filePath);
    m_logFile.clear();
}

FileSink::~FileSink() { flush(); }

void FileSink::Log(const LogMessage& message)
{
    m_buffer.push_back(message.toString(false));
    if (m_buffer.size() >= m_flushThreshold) { flush(); }
}

void FileSink::flush()
{
    if (m_buffer.empty()) return;
    m_logFile.appendLines(m_buffer, false);
    m_buffer.clear();
}

}   // namespace Engine
