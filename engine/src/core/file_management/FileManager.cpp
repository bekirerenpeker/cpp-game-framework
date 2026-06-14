#include "core/file_management/FileManager.hpp"
#include <fstream>

namespace Engine {

bool FileManager::doesPathExist(fs::path path) { return fs::exists(path); }
bool FileManager::isDirectory(fs::path path) { return fs::is_directory(path); }

bool FileManager::createFile(fs::path path)
{
    if (exists(path)) {
        LOG_WARNING("file with path {} already exists", path);
        return false;
    }

    std::error_code ec;
    if (path.has_parent_path()) {
        fs::create_directories(path.parent_path(), ec);
        if (ec) return false;
    }

    std::ofstream file(path);
    if (!file.is_open()) return false;

    file.close();
    return true;
}

bool FileManager::createFolder(fs::path path)
{
    if (fs::exists(path)) {
        LOG_WARNING("folder with path {} already exists", path);
        return true;
    }

    std::error_code ec;
    fs::create_directories(path, ec);
    return !ec;
}

fs::path FileManager::getCurrentFolder() { return fs::current_path(); }

}   // namespace Engine
