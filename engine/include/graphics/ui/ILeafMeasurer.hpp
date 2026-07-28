#pragma once

#include "utils/TypeAliases.hpp"
#include <vector>

namespace Engine {

struct LeafWidths
{
    float min = 0.0f;
    float max = 0.0f;
};

// byteOffset/byteCount index into the leaf's own source string, so a later content
// pass can redraw a wrapped line without repeating the break search.
struct LayoutLine
{
    uint byteOffset = 0;
    uint byteCount = 0;
    float width = 0.0f;
    float height = 0.0f;
};

class ILeafMeasurer
{
  public:
    virtual ~ILeafMeasurer() = default;

    virtual LeafWidths measureWidths() = 0;
    virtual float measureHeight(float contentWidth, std::vector<LayoutLine>& lines) = 0;
};

}   // namespace Engine
