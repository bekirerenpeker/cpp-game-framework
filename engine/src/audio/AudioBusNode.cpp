#include "audio/AudioBusNode.hpp"
#include "audio/AudioManager.hpp"
#include <algorithm>

namespace Engine {

AudioBusNode::AudioBusNode(const std::string& name, AudioBusNode* parent, PlaybackOptions options)
    : m_name(name), m_parent(parent), m_options(options)
{
    m_options.clampToValidRange();
    parent->m_children.push_back(this);
}

AudioBusNode::~AudioBusNode()
{
    if (!m_parent) return;
    auto it = std::find(m_parent->m_children.begin(), m_parent->m_children.end(), this);
    if (it != m_parent->m_children.end()) m_parent->m_children.erase(it);
}

PlaybackOptions AudioBusNode::getOptions() const { return m_options; }
const std::vector<AudioBusNode*>& AudioBusNode::getChildren() const { return m_children; }
const std::string& AudioBusNode::getName() const { return m_name; }

void AudioBusNode::setOptions(const PlaybackOptions& options)
{
    m_options = options;
    m_options.clampToValidRange();
    updateChildrenOptions();
}

void AudioBusNode::updateChildrenOptions()
{
    updateInstanceOptions();

    for (auto& child : m_children) {
        child->m_options.combine(m_options);
        child->updateChildrenOptions();
    }
}

void AudioBusNode::updateInstanceOptions()
{
    // loop over all instances to update their options
    // this funcion may be left empty if we dont want the already existing instances options to not change to avoid performance costs
    // or only change streamed audio's options since their are the only ones that are long
}

}   // namespace Engine
