#include "audio/AudioBusNode.hpp"
#include "audio/AudioManager.hpp"
#include <algorithm>

namespace Engine {

AudioBusNode::AudioBusNode(
    const std::string& name, size_t index, size_t parentIndex, const PlaybackOptions& options
)
    : m_name(name), m_index(index), m_parentIndex(parentIndex)
{
    AudioBusNode* parent =
        (m_parentIndex != -1) ? AudioManager::get().getBusNode(m_parentIndex) : nullptr;
    if (parent) parent->m_childIndices.push_back(index);
    setOptions(options);
}

PlaybackOptions AudioBusNode::getOptions() const { return m_coreOptions; }
PlaybackOptions AudioBusNode::getCombinedOptions() const { return m_combinedOptions; }
const std::string& AudioBusNode::getName() const { return m_name; }

void AudioBusNode::setOptions(const PlaybackOptions& options)
{
    AudioBusNode* parent =
        (m_parentIndex != -1) ? AudioManager::get().getBusNode(m_parentIndex) : nullptr;

    m_coreOptions = options;
    m_coreOptions.clampToValidRange();

    m_combinedOptions = parent ? options.combined(parent->m_combinedOptions) : options;
    m_combinedOptions.clampToValidRange();

    updateChildrenOptions();
}
void AudioBusNode::updateChildrenOptions()
{
    for (auto& childIndex : m_childIndices) {
        AudioBusNode* child = AudioManager::get().getBusNode(childIndex);
        child->m_combinedOptions = child->m_coreOptions.combined(m_combinedOptions);
        child->updateChildrenOptions();
    }
}

}   // namespace Engine
