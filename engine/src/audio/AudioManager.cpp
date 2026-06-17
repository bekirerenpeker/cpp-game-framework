#include "audio/AudioManager.hpp"
#include "core/logging/LoggerMacros.hpp"

namespace Engine {

AudioManager::AudioManager() { m_busNodes["Master"] = AudioBusNode("Master"); }

void AudioManager::addBusNode(
    const std::string& name, const std::string& parentName, const PlaybackOptions& initalOptions
)
{
    if (!m_busNodes.count(parentName)) {
        LOG_ERROR("trying to insert a new bus node under {} but it doesn't exist", parentName);
        return;
    }
    AudioBusNode* parent = &m_busNodes[parentName];
    m_busNodes[name] = AudioBusNode(name, parent, initalOptions);
}
AudioBusNode* AudioManager::getBusNode(const std::string& name)
{
    return m_busNodes.count(name) ? &m_busNodes[name] : nullptr;
}
bool AudioManager::removeBusNode(const std::string& name)
{
    if (!m_busNodes.count(name)) return false;

    std::queue<std::string> queue;
    queue.push(name);

    while (queue.size()) {
        AudioBusNode& curr = m_busNodes[queue.back()];
        for (AudioBusNode* child : curr.getChildren()) queue.push(child->getName());
        m_busNodes.erase(curr.getName());
        queue.pop();
    }

    return true;
}

}   // namespace Engine
