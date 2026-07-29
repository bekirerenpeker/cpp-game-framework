#pragma once

#include "utils/TypeAliases.hpp"
#include <vector>

namespace Engine {

struct LeafWidths
{
    float min = 0.0f;
    float max = 0.0f;
};

// One laid-out line. height is the step to the next line, which is the tallest
// style on this line rather than any single style's; ascent places the baseline
// inside that step. descent is positive downwards.
struct LayoutLine
{
    float width = 0.0f;
    float height = 0.0f;
    float ascent = 0.0f;
    float descent = 0.0f;
};

class ILeafMeasurer
{
  public:
    virtual ~ILeafMeasurer() = default;

    virtual LeafWidths measureWidths() = 0;
    virtual float measureHeight(float contentWidth, std::vector<LayoutLine>& lines) = 0;
};

}   // namespace Engine
