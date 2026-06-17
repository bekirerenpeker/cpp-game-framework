#include "EngineInclude.hpp"

using namespace Engine;

int main()
{
    Logger::get().addSink<FileSink>("log.txt");

    fs::path imgPath = "test_image.jpg";
    if (!FileManager::get().doesPathExist(imgPath)) FileManager::get().createFile(imgPath);

    float freq = 6;
    ImageData imgData(1000, 1000, 3);
    for (int x = 0; x < imgData.width; x++) {
        for (int y = 0; y < imgData.height; y++) {
            float r = Math::perlin2D(
                (x / (float)imgData.width) * freq, (y / (float)imgData.height) * freq, 0
            );
            float g = Math::perlin2D(
                (x / (float)imgData.width) * freq, (y / (float)imgData.height) * freq, 1
            );
            float b = Math::perlin2D(
                (x / (float)imgData.width) * freq, (y / (float)imgData.height) * freq, 2
            );
            imgData.setValueAt(x, y, 0, r * 256);
            imgData.setValueAt(x, y, 1, g * 256);
            imgData.setValueAt(x, y, 2, b * 256);
        }
    }

    IdType imgId = ResourceManager::get().addResource<ImageFile>(imgPath);
    ImageFile* imgFile = ResourceManager::get().getResource<ImageFile>(imgId);
    imgFile->saveImage(imgData);
    ResourceManager::get().unloadResource(imgId);

    return 0;
}
