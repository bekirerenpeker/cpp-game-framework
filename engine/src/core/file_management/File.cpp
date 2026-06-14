#include "core/file_management/File.hpp"
#include <system_error>

namespace Engine {

File::File(fs::path path, FileType fileType) : IFileEntry(path, fileType)
{
    if (fs::is_directory(path)) {
        m_path = "";
        m_fileType = FileType::None;
        m_isValid = false;
    }
}

std::string File::getNameWithoutExt() const { return m_path.stem().string(); }
std::string File::getExtension() const { return m_path.extension().string(); }

size_t File::getSize() const
{
    std::error_code ec;
    auto size = fs::file_size(m_path, ec);
    if (ec) return 0;
    return size;
}

bool File::renameFilename(const std::string& filenameWithoutExt)
{
    std::string currentExt = m_path.extension().string();
    return rename(filenameWithoutExt + currentExt);
}

bool File::renameExtension(const std::string& extensionName)
{
    fs::path tempName = m_path.filename();
    tempName.replace_extension(extensionName);
    return rename(tempName.string());
}

}   // namespace Engine
