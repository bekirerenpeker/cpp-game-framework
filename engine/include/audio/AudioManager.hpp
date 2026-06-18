#pragma once

#include "utils/Singleton.hpp"
#include "utils/IdIndexedVector.hpp"
#include "audio/AudioInstance.hpp"
#include "audio/AudioBusNode.hpp"
#include <unordered_map>
#include <mutex>

namespace Engine {

class AudioManager : public Singleton<AudioManager>
{
    friend class Singleton<AudioManager>;

  private:
    ma_device m_device;
    std::unordered_map<std::string, AudioBusNode*> m_busNodes;
    IdIndexedVector<AudioInstance> m_instances;
    std::mutex m_audioMutex;

  public:
    void shutdown();

    void addBusNode(
        const std::string& name, const std::string& parentName = "Master",
        const PlaybackOptions& initalOptions = {}
    );
    bool removeBusNode(const std::string& name);
    AudioBusNode* getBusNode(const std::string& name);

    IdType playAudio(
        IAudioSource* source, const std::string& busName = "Master",
        const PlaybackOptions& options = {}
    );
    void stopAudioInstance(IdType id);
    AudioInstance* getAudioInstance(IdType id);

    // thread safe set and get functions
    ma_uint64 getCursorFrames(IdType id);
    float getCursorSeconds(IdType id);

    void setCursorFrames(IdType id, ma_uint64 cursor);
    void setCursorSeconds(IdType id, float seconds);

    ma_uint64 getDurationFrames(IdType id);
    float getDurationSeconds(IdType id);

    bool isPaused(IdType id);
    void setIsPaused(IdType id, bool isPaused);

    PlaybackOptions getOptions(IdType id);
    void setOptions(IdType id, const PlaybackOptions& options);

  private:
    AudioManager();
    ~AudioManager();

    static void
    data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
};

};   // namespace Engine
