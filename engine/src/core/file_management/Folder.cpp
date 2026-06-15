#include "core/file_management/Folder.hpp"
#include "core/file_management/File.hpp"

namespace Engine {

Folder::Folder(fs::path path, FileType fileType) : IFileEntry(path, fileType)
{
    if (!FileManager::get().isDirectory(path)) makeInvalid();
}

std::vector<std::unique_ptr<IFileEntry>> Folder::getChildren() const
{
    std::vector<std::unique_ptr<IFileEntry>> children;
    if (!m_isValid) return children;

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(m_path, ec)) {
        if (entry.is_directory(ec)) {
            children.push_back(std::make_unique<Folder>(entry.path()));
        } else if (entry.is_regular_file(ec)) {
            children.push_back(std::make_unique<File>(entry.path()));
        }
    }

    return children;
}

size_t Folder::getSize() const
{
    size_t sizeSum = 0;
    for (const auto& child : getChildren()) sizeSum += child->getSize();
    return sizeSum;
}

}   // namespace Engine
