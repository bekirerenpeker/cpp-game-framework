#pragma once

#include <filesystem>

namespace Engine {

namespace fs = std::filesystem;

enum class FileType
{
    None,
    Folder,
    File,
    BinaryFile,
    TextFile,
    ImageFile,
};

class IFileEntry
{
  protected:
    FileType m_fileType = FileType::None;
    fs::path m_path;
    bool m_isValid;

  public:
    IFileEntry(const fs::path& path, FileType fileType);
    virtual ~IFileEntry() = default;

    FileType getType() const;
    bool isValid() const;

    fs::path getPath() const;
    std::string getName() const;

    virtual size_t getSize() const = 0;

    bool moveTo(const fs::path& parentFolder);
    bool rename(const std::string& nameWithExt);
    bool deleteFromDisk();
};

}   // namespace Engine
