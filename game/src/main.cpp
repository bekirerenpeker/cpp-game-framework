#include "EngineInclude.hpp"

using namespace Engine;

// TODO: fix the audio stream it gradually lags and crashes
// TODO: cant change options while stream audio is still playing

int main()
{
    Logger::get().addSink<FileSink>("game/output/log.txt");

    IdType windowId = WindowManager::get().createWindow({1000, 800, "test window"});

    AudioManager::get();   // initalize the audio engine
    IdType soundId =
        ResourceManager::get().addResource<AudioBuffer>("game/assets/audio/CoinPickup.wav");
    IdType musicId = ResourceManager::get().addResource<AudioBuffer>("game/assets/audio/Music.mp3");
    auto* sound = ResourceManager::get().getResource<AudioBuffer>(soundId);
    auto* music = ResourceManager::get().getResource<AudioBuffer>(musicId);
    IdType musicInstanceId = AudioManager::get().playAudio(music);

    Window* mainWindow = WindowManager::get().getMainWindow();
    while (mainWindow->isOpen()) {
        Input::get().update(windowId);

        if (Input::get().keyPressed(KeyCode::Q)) break;
        if (Input::get().keyPressed(KeyCode::S)) AudioManager::get().playAudio(sound);
        if (Input::get().keyPressed(KeyCode::C)) {
            AudioManager::get().stopAudioInstance(musicInstanceId);
        }

        AudioInstance* inst = AudioManager::get().getAudioInstance(musicInstanceId);
        if (inst) {
            if (Input::get().keyPressed(KeyCode::Space)) inst->setIsPaused(!inst->isPaused());

            if (Input::get().keyPressed(KeyCode::Right))
                inst->setCursorSeconds(inst->getCursorSeconds() + 10);
            if (Input::get().keyPressed(KeyCode::Left))
                inst->setCursorSeconds(inst->getCursorSeconds() - 10);
        }

        mainWindow->swapBuffers();
        GlfwContext::pollEvents();
    }

    AudioManager::get().shutdown();   // terminate it before the resource manager terminates
    return 0;
}
