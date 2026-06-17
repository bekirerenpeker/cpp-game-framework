#include "audio/AudioManager.hpp"
#include "core/logging/LoggerMacros.hpp"

namespace Engine {

AudioManager::AudioManager() { m_busNodes["Master"] = new AudioBusNode("Master"); }
AudioManager::~AudioManager()
{
    for (auto& [name, nodePtr] : m_busNodes) delete nodePtr;
    m_busNodes.clear();
}

void AudioManager::addBusNode(
    const std::string& name, const std::string& parentName, const PlaybackOptions& initalOptions
)
{
    if (!m_busNodes.count(parentName)) {
        LOG_ERROR("trying to insert a new bus node under {} but it doesn't exist", parentName);
        return;
    }
    AudioBusNode* parent = m_busNodes[parentName];
    m_busNodes[name] = new AudioBusNode(name, parent, initalOptions);
}
AudioBusNode* AudioManager::getBusNode(const std::string& name)
{
    return m_busNodes.count(name) ? m_busNodes[name] : nullptr;
}
bool AudioManager::removeBusNode(const std::string& name)
{
    if (!m_busNodes.count(name)) return false;

    std::queue<std::string> queue;
    queue.push(name);

    while (!queue.empty()) {
        std::string currName = queue.front();
        queue.pop();

        AudioBusNode* currNode = m_busNodes[currName];
        for (AudioBusNode* child : currNode->getChildren()) { queue.push(child->getName()); }

        m_busNodes.erase(currName);
        delete currNode;
    }

    return true;
}

IdType AudioManager::playAudio(
    IAudioSource* source, const std::string& busName, const PlaybackOptions& options
)
{
    if (!m_busNodes.count(busName)) {
        LOG_ERROR("trying to play audio under bus {} but it doesn't exist", busName);
        return INVALID_ID;
    }
    AudioBusNode* busNode = m_busNodes[busName];
    return m_instances.add(AudioInstance(source, options.combined(busNode->getOptions())));
}
void AudioManager::stopAudioInstance(IdType id) { m_instances.remove(id); }
AudioInstance* AudioManager::getAudioInstance(IdType id) { return m_instances.get(id); }
const std::vector<std::pair<IdType, AudioInstance>>& AudioManager::getAllInstances() const
{
    return m_instances.getAll();
}

}   // namespace Engine
