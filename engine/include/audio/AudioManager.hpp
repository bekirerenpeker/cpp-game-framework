#pragma once

#include "utils/Singleton.hpp"
#include "utils/IdIndexedVector.hpp"
#include "audio/AudioInstance.hpp"
#include "audio/AudioBusNode.hpp"
#include <unordered_map>

namespace Engine {

class AudioManager : public Singleton<AudioManager>
{
    friend class Singleton<AudioManager>;

  private:
    std::unordered_map<std::string, AudioBusNode*> m_busNodes;
    IdIndexedVector<AudioInstance> m_instances;

  public:
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
    const std::vector<std::pair<IdType, AudioInstance>>& getAllInstances() const;

  private:
    AudioManager();
    ~AudioManager();
};

};   // namespace Engine
