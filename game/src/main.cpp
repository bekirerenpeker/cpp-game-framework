#include "EngineInclude.hpp"
#include "piano_demo.hpp"

using namespace Engine;

int main()
{
    Logger::get().addSink<FileSink>("game/output/log.txt");

    IdType windowId = WindowManager::get().createWindow({1000, 800, "test window"});

    AudioManager::get();
    AudioManager::get().addBusNode("Music");
    AudioManager::get().addBusNode("Menu Music", "Music");

    IdType streamSoundId =
        ResourceManager::get().addResource<AudioStream>("game/assets/audio/Music.mp3");
    auto* streamSound = ResourceManager::get().getResource<AudioStream>(streamSoundId);
    IdType musicInstId = AudioManager::get().playAudio(streamSound, "Menu Music");

    Window* mainWindow = WindowManager::get().getMainWindow();
    while (mainWindow->isOpen()) {
        Time::get().update();
        Input::get().update(windowId);

        if (Input::get().keyPressed(KeyCode::Q)) break;

        auto busNode = AudioManager::get().getBusNode("Music");
        if (busNode) {
            if (Input::get().keyPressed(KeyCode::Up)) {
                busNode->setOptions({busNode->getOptions().volume + 0.1f, 1, 0, false});
            }
            if (Input::get().keyPressed(KeyCode::Down)) {
                busNode->setOptions({busNode->getOptions().volume - 0.1f, 1, 0, false});
            }
        }

        mainWindow->swapBuffers();
        GlfwContext::pollEvents();
    }

    AudioManager::get().shutdown();   // terminate it before the resource manager terminates
    return 0;
}
