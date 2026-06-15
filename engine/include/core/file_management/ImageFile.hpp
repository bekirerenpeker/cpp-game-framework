#pragma once

#include "core/file_management/File.hpp"
#include "utils/ImageData.hpp"

namespace Engine {

class ImageFile : public File
{
  private:
    ImageData m_imgData;

  public:
    ImageFile(const fs::path& path);
    ~ImageFile() override = default;

    size_t getWidth() const;
    size_t getHeight() const;
    size_t getDepth() const;
    size_t getBufferSize() const;
    byte* getPixels() const;

    bool loadImage();
    bool saveImage(ImageData& imgData);
};

};   // namespace Engine
