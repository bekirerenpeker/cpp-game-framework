#pragma once

#include "utils/TypeAliases.hpp"
#include "audio/PlaybackOptions.hpp"
#include <string>
#include <vector>

namespace Engine {

class AudioBusNode
{
  private:
    std::string m_name;
    PlaybackOptions m_options;

    AudioBusNode* m_parent = nullptr;
    std::vector<AudioBusNode*> m_children;

  public:
    AudioBusNode(
        const std::string& name, AudioBusNode* parent = nullptr, PlaybackOptions options = {}
    );
    ~AudioBusNode();

    PlaybackOptions getOptions() const;
    const std::vector<AudioBusNode*>& getChildren() const;
    const std::string& getName() const;

    void setOptions(const PlaybackOptions& options);

  private:
    void updateChildrenOptions();
};

}   // namespace Engine
