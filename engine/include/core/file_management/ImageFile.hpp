#pragma once

#include "core/file_management/File.hpp"
#include "utils/ImageData.hpp"

namespace Engine {

class ImageFile : public File
{
  private:
    ImageType m_imgType;
    ImageData m_imgData;

  public:
    ImageFile(const fs::path& path);
    ~ImageFile() override = default;

    size_t getWidth() const;
    size_t getHeight() const;
    size_t getDepth() const;
    size_t getBufferSize() const;
    byte* getPixels() const;
    const ImageData& getImageData() const;
    ImageData& getImageData();

    bool loadImage(int desiredChannels = 0);
    bool saveImage(ImageData& imgData);
};

};   // namespace Engine
