#pragma once

#include "core/file_management/IFileEntry.hpp"

namespace Engine {

class File : public IFileEntry
{
  public:
    File(fs::path path, FileType fileType = FileType::File);
    virtual ~File() = default;

    std::string getNameWithoutExt() const;
    std::string getExtension() const;

    size_t getSize() const override;

    bool renameFilename(const std::string& filenameWithoutExt);
    bool renameExtension(const std::string& extensionName);
};

}   // namespace Engine
