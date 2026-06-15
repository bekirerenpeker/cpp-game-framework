#include "EngineInclude.hpp"

using namespace Engine;

int main()
{
    Logger::get().addSink<FileSink>("log.txt");

    float freq = 10.f;
    ImageData imgData(1000, 1000, 1);
    for (int x = 0; x < imgData.width; x++) {
        for (int y = 0; y < imgData.height; y++) {
            imgData.setValueAt(
                x, y, 0,
                Math::perlin2D(x / (float)imgData.width * freq, y / (float)imgData.height * freq) *
                    256
            );
        }
    }
    FileManager::get().createFile("test_image.png");
    ImageFile file("test_image.png");
    file.saveImage(imgData);

    return 0;
}
