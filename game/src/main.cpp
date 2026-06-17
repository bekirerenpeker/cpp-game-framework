#include "EngineInclude.hpp"

using namespace Engine;

// TODO: fix the audio stream it gradually lags and crashes
// TODO: cant change options while stream audio is still playing

int main()
{
    Logger::get().addSink<FileSink>("game/output/log.txt");

    IdType windowId = WindowManager::get().createWindow({1000, 800, "test window"});

    AudioManager::get();
    IdType soundId =
        ResourceManager::get().addResource<AudioBuffer>("game/assets/audio/CoinPickup.wav");
    IdType musicId = ResourceManager::get().addResource<AudioStream>("game/assets/audio/Music.mp3");
    AudioBuffer* audio = ResourceManager::get().getResource<AudioBuffer>(soundId);
    AudioStream* music = ResourceManager::get().getResource<AudioStream>(musicId);

    IdType musicInstanceId = AudioManager::get().playAudio(music);

    Window* mainWindow = WindowManager::get().getMainWindow();
    while (mainWindow->isOpen()) {
        Input::get().update(windowId);

        if (Input::get().keyPressed(KeyCode::Q)) break;

        if (Input::get().keyPressed(KeyCode::S)) AudioManager::get().playAudio(audio);

        if (Input::get().keyPressed(KeyCode::C)) {
            AudioManager::get().stopAudioInstance(musicInstanceId);
        }

        AudioInstance* inst = AudioManager::get().getAudioInstance(musicInstanceId);
        if (inst) {
            if (Input::get().keyPressed(KeyCode::Space)) inst->setIsPaused(!inst->isPaused());
            if (Input::get().keyPressed(KeyCode::Up)) inst->setOptions({2, 1, 0, false});
            if (Input::get().keyPressed(KeyCode::Down)) inst->setOptions({1, 1, 0, false});
        }

        mainWindow->swapBuffers();
        GlfwContext::pollEvents();
    }

    return 0;
}
