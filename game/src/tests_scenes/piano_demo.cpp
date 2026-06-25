#include "EngineInclude.hpp"
#include "test_funcs.hpp"

using namespace Engine;

int piano_demo()
{
    IdType windowId = WindowManager::get().createWindow({1000, 800, "test window"});

    KeyCode keyCodes[13] = {KeyCode::A, KeyCode::W, KeyCode::S, KeyCode::D, KeyCode::R,
                            KeyCode::F, KeyCode::T, KeyCode::G, KeyCode::H, KeyCode::U,
                            KeyCode::J, KeyCode::I, KeyCode::K};
    fs::path dir = "game/assets/audio/piano_notes";
    fs::path files[13] = {"a1.wav", "a1s.wav", "b1.wav",  "c1.wav", "c1s.wav", "d1.wav", "d1s.wav",
                          "e1.wav", "f1.wav",  "f1s.wav", "g1.wav", "g1s.wav", "c2.wav"};
    AudioBuffer* resources[13] = {nullptr};
    IdType instances[13] = {INVALID_ID};

    AudioManager::get();   // initalize the audio engine
    for (int i = 0; i < 13; i++) {
        IdType id = ResourceManager::get().addResource<AudioBuffer>(dir / files[i]);
        resources[i] = ResourceManager::get().getResource<AudioBuffer>(id);
    }

    Window* mainWindow = WindowManager::get().getMainWindow();
    while (mainWindow->isOpen()) {
        Time::get().update();
        Input::get().update(windowId);

        if (Input::get().keyPressed(KeyCode::Q)) break;

        for (int i = 0; i < 13; i++) {
            if (Input::get().keyPressed(keyCodes[i])) {
                instances[i] = AudioManager::get().playAudio(resources[i]);
            }
            if (Input::get().keyReleased(keyCodes[i]) &&
                AudioManager::get().getAudioInstance(instances[i])) {
                // AudioManager::get().stopAudioInstance(instances[i]);
            }
        }

        mainWindow->swapBuffers();
        GlfwContext::pollEvents();
    }

    AudioManager::get().shutdown();   // terminate it before the resource manager terminates
    return 0;
}
