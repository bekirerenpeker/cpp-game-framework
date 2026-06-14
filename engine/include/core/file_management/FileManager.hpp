#pragma once

#include "utils/Singleton.hpp"
#include "core/logging/LoggerMacros.hpp"
#include <filesystem>

namespace Engine {

namespace fs = std::filesystem;

class FileManager : public Singleton<FileManager>
{
    friend class Singleton<FileManager>;

  public:
    bool doesPathExist(fs::path path);
    bool isDirectory(fs::path path);

    bool createFile(fs::path path);
    bool createFolder(fs::path path);

    fs::path getCurrentFolder();

  private:
    FileManager() = default;
    ~FileManager() = default;
};

}   // namespace Engine

DEFINE_TYPE_FORMATTER(std::filesystem::path, "{}", obj.string());
