#pragma once

#include "utils/Singleton.hpp"
#include "audio/AudioBusNode.hpp"
#include <unordered_map>

namespace Engine {

class AudioManager : public Singleton<AudioManager>
{
    friend class Singleton<AudioManager>;

  private:
    std::unordered_map<std::string, AudioBusNode> m_busNodes;

  public:
    void addBusNode(
        const std::string& name, const std::string& parentName = "Master",
        const PlaybackOptions& initalOptions = {}
    );
    bool removeBusNode(const std::string& name);
    AudioBusNode* getBusNode(const std::string& name);

  private:
    AudioManager();
    ~AudioManager() = default;
};

};   // namespace Engine
