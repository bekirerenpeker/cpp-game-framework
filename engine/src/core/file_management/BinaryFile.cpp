#include "core/file_management/BinaryFile.hpp"

namespace Engine {

BinaryFile::BinaryFile(const fs::path& path) : File(path, FileType::BinaryFile) {}

BinaryFile::~BinaryFile() { close(); }

bool BinaryFile::clear()
{
    bool wasOpen = m_stream.is_open();
    if (wasOpen) close();
    bool success = File::clear();
    if (wasOpen) open();
    return success;
}

bool BinaryFile::open(std::ios::openmode mode)
{
    if (m_stream.is_open()) { close(); }

    m_stream.open(m_path, mode);
    return m_stream.is_open();
}

void BinaryFile::close()
{
    if (m_stream.is_open()) m_stream.close();
}

bool BinaryFile::isOpen() const { return m_stream.is_open(); }

void BinaryFile::seekRead(size_t position)
{
    if (m_stream.is_open()) m_stream.seekg(position, std::ios::beg);
}

void BinaryFile::seekWrite(size_t position)
{
    if (m_stream.is_open()) m_stream.seekp(position, std::ios::beg);
}

size_t BinaryFile::getReadPosition()
{
    return m_stream.is_open() ? static_cast<size_t>(m_stream.tellg()) : 0;
}

size_t BinaryFile::getWritePosition()
{
    return m_stream.is_open() ? static_cast<size_t>(m_stream.tellp()) : 0;
}

bool BinaryFile::writeBytes(const uint8_t* data, size_t size)
{
    if (!m_stream.is_open() || !data || size == 0) return false;

    m_stream.write(reinterpret_cast<const char*>(data), size);
    return m_stream.good();
}

bool BinaryFile::readBytes(uint8_t* buffer, size_t size)
{
    if (!m_stream.is_open() || !buffer || size == 0) return false;

    m_stream.read(reinterpret_cast<char*>(buffer), size);
    return m_stream.good();
}

}   // namespace Engine
