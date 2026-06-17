#include "EngineInclude.hpp"

using namespace Engine;

int main()
{
    Logger::get().addSink<FileSink>("game/output/log.txt");

    IdType windowId = WindowManager::get().createWindow({1000, 800, "test window"});

    AudioManager::get();
    IdType soundId =
        ResourceManager::get().addResource<AudioBuffer>("game/assets/audio/CoinPickup.wav");
    AudioBuffer* audio = ResourceManager::get().getResource<AudioBuffer>(soundId);

    Window* mainWindow = WindowManager::get().getMainWindow();
    while (mainWindow->isOpen()) {
        Input::get().update(windowId);

        if (Input::get().keyPressed(KeyCode::Q)) break;
        if (Input::get().keyPressed(KeyCode::S)) AudioManager::get().playAudio(audio);

        mainWindow->swapBuffers();
        GlfwContext::pollEvents();
    }

    return 0;
}
