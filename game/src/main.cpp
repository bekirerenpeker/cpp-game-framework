#include "EngineInclude.hpp"

using namespace Engine;

int main()
{
    Logger::get().addSink<FileSink>("game/output/log.txt");

    IdType windowId = WindowManager::get().createWindow({1000, 800, "test window"});

    AudioManager::get();   // initalize the audio engine
    IdType soundId =
        ResourceManager::get().addResource<AudioBuffer>("game/assets/audio/CoinPickup.wav");
    IdType musicId = ResourceManager::get().addResource<AudioStream>("game/assets/audio/Music.mp3");
    auto* sound = ResourceManager::get().getResource<AudioBuffer>(soundId);
    auto* music = ResourceManager::get().getResource<AudioStream>(musicId);
    IdType musicInstanceId = AudioManager::get().playAudio(music);

    Window* mainWindow = WindowManager::get().getMainWindow();
    while (mainWindow->isOpen()) {
        Time::get().update();
        Input::get().update(windowId);

        if (Input::get().keyPressed(KeyCode::Q)) break;
        if (Input::get().keyPressed(KeyCode::S)) AudioManager::get().playAudio(sound);
        if (Input::get().keyPressed(KeyCode::C)) {
            AudioManager::get().stopAudioInstance(musicInstanceId);
        }

        if (AudioManager::get().getAudioInstance(musicInstanceId)) {
            LOG_INFO(
                "{}/{}", AudioManager::get().getCursorSeconds(musicInstanceId),
                AudioManager::get().getDurationSeconds(musicInstanceId)
            );

            if (Input::get().keyPressed(KeyCode::Space)) {
                bool paused = AudioManager::get().isPaused(musicInstanceId);
                AudioManager::get().setIsPaused(musicInstanceId, !paused);
            }

            if (Input::get().keyPressed(KeyCode::Right)) {
                float current = AudioManager::get().getCursorSeconds(musicInstanceId);
                AudioManager::get().setCursorSeconds(musicInstanceId, current + 10.0f);
            }
            if (Input::get().keyPressed(KeyCode::Left)) {
                float current = AudioManager::get().getCursorSeconds(musicInstanceId);
                AudioManager::get().setCursorSeconds(musicInstanceId, current - 10.0f);
            }
        }

        mainWindow->swapBuffers();
        GlfwContext::pollEvents();
    }

    AudioManager::get().shutdown();   // terminate it before the resource manager terminates
    return 0;
}
