#include "core/file_management/ImageFile.hpp"
#include "stb/stb_image_write.h"
#include "stb/stb_image.h"
#include "core/logging/LoggerMacros.hpp"

namespace Engine {

ImageFile::ImageFile(const fs::path& path)
    : File(path, FileType::ImageFile), m_imgType(FileManager::get().getImageTypeFromPath(path))
{
    if (m_imgType == ImageType::None) {
        LOG_WARNING("unsupported extension for images {}", path.extension().string());
        makeInvalid();
    }
}

size_t ImageFile::getWidth() const { return m_imgData.width; }
size_t ImageFile::getHeight() const { return m_imgData.height; }
size_t ImageFile::getDepth() const { return m_imgData.depth; }
size_t ImageFile::getBufferSize() const { return getWidth() * getHeight() * getDepth(); }
byte* ImageFile::getPixels() const { return m_imgData.pixels; }

bool ImageFile::loadImage()
{
    if (!m_isValid) return false;

    int width, height, channels;
    unsigned char* rawPixels = stbi_load(m_path.string().c_str(), &width, &height, &channels, 0);

    if (!rawPixels) {
        LOG_WARNING("couldn't read image data from {}", m_path);
        makeInvalid();
        return false;
    }

    m_imgData.width = width, m_imgData.height = height, m_imgData.depth = channels;
    m_imgData.pixels = rawPixels;
    return true;
}

bool ImageFile::saveImage(ImageData& imgData)
{
    if (!m_isValid) return false;

    int success = 0;
    switch (m_imgType) {
    case ImageType::Png:
        success = stbi_write_png(
            m_path.string().c_str(), static_cast<int>(imgData.width),
            static_cast<int>(imgData.height), static_cast<int>(imgData.depth), imgData.pixels,
            static_cast<int>(imgData.width * imgData.depth)
        );
        break;

    case ImageType::Jpeg:
        success = stbi_write_jpg(
            m_path.string().c_str(), static_cast<int>(imgData.width),
            static_cast<int>(imgData.height), static_cast<int>(imgData.depth), imgData.pixels, 90
        );
        break;

    default: LOG_ERROR("unsopported image type {}", m_path.extension().string()); return;
    }

    m_imgData = std::move(imgData);
    return success != 0;
}

}   // namespace Engine
