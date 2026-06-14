#pragma once

#include "core/file_management/IFileEntry.hpp"

namespace Engine {

class Folder : public IFileEntry
{
  public:
    Folder(fs::path path, FileType fileType = FileType::Folder);
    virtual ~Folder() override = default;

    std::vector<std::unique_ptr<IFileEntry>> getChildren() const;

    size_t getSize() const override;
};

}   // namespace Engine
