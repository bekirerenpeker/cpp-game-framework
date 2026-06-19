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
    PlaybackOptions m_coreOptions, m_combinedOptions;

    size_t m_index;
    size_t m_parentIndex = -1;
    std::vector<size_t> m_childIndices;

  public:
    AudioBusNode(
        const std::string& name, size_t index, size_t parentIndex,
        const PlaybackOptions& options = {}
    );
    ~AudioBusNode() = default;

    PlaybackOptions getOptions() const;
    PlaybackOptions getCombinedOptions() const;
    const std::string& getName() const;

    void setOptions(const PlaybackOptions& options);

  private:
    void updateChildrenOptions();
};

}   // namespace Engine
