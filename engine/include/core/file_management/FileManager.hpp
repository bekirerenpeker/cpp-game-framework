#pragma once

#include "utils/Singleton.hpp"
#include "utils/ImageData.hpp"
#include "core/logging/LoggerMacros.hpp"
#include <filesystem>

namespace Engine {

namespace fs = std::filesystem;

enum class ImageType
{
    None,
    Png,
    Jpeg,
};

class FileManager : public Singleton<FileManager>
{
    friend class Singleton<FileManager>;

  public:
    bool doesPathExist(const fs::path& path);
    bool isDirectory(const fs::path& path);

    bool createFile(const fs::path& path);
    bool createFolder(const fs::path& path);
    bool createImageFile(const fs::path& path, const ImageData& imgData);

    ImageType getImageTypeFromPath(const fs::path& path);

    fs::path getCurrentFolder();

  private:
    FileManager() = default;
    ~FileManager() = default;
};

}   // namespace Engine

DEFINE_TYPE_FORMATTER(std::filesystem::path, "{}", obj.string());
