#include "core/file_management/FileManager.hpp"
#include "stb/stb_image_write.h"
#include <fstream>

namespace Engine {

bool FileManager::doesPathExist(const fs::path& path) { return fs::exists(path); }
bool FileManager::isDirectory(const fs::path& path) { return fs::is_directory(path); }

bool FileManager::createFile(const fs::path& path)
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

bool FileManager::createFolder(const fs::path& path)
{
    if (fs::exists(path)) {
        LOG_WARNING("folder with path {} already exists", path);
        return true;
    }

    std::error_code ec;
    fs::create_directories(path, ec);
    return !ec;
}

bool FileManager::createImageFile(const fs::path& path, const ImageData& imgData)
{
    if (!createFile(path)) return false;
    int success = stbi_write_png(
        path.string().c_str(), static_cast<int>(imgData.width), static_cast<int>(imgData.height),
        static_cast<int>(imgData.depth), imgData.pixels,
        static_cast<int>(imgData.width * imgData.depth)
    );
    return success != 0;
}

fs::path FileManager::getCurrentFolder() { return fs::current_path(); }

}   // namespace Engine
