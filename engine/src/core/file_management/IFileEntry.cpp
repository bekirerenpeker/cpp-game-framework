#include "core/file_management/IFileEntry.hpp"
#include <system_error>

namespace Engine {

IFileEntry::IFileEntry(const fs::path& path, FileType fileType)
    : m_path(path), m_fileType(fileType), m_isValid(true)
{
    if (!FileManager::get().doesPathExist(path)) {
        m_path = "";
        m_fileType = FileType::None;
        m_isValid = false;
    }
}

FileType IFileEntry::getType() const { return m_fileType; }
bool IFileEntry::isValid() const { return m_isValid; }

fs::path IFileEntry::getPath() const { return m_path; }
std::string IFileEntry::getName() const { return m_path.filename().string(); }

bool IFileEntry::moveTo(const fs::path& parentFolder)
{
    fs::path newPath = parentFolder / m_path.filename();

    std::error_code ec;
    fs::rename(m_path, newPath, ec);

    if (ec) return false;

    m_path = newPath;
    return true;
}

bool IFileEntry::rename(const std::string& nameWithExt)
{
    fs::path newPath = m_path.parent_path() / nameWithExt;

    std::error_code ec;
    fs::rename(m_path, newPath, ec);

    if (ec) return false;

    m_path = newPath;
    return true;
}

bool IFileEntry::deleteFromDisk()
{
    std::error_code ec;
    std::uintmax_t deletedCount = fs::remove_all(m_path, ec);

    if (ec || deletedCount == 0) return false;

    m_isValid = false;
    return true;
}

}   // namespace Engine
