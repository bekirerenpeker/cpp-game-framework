#pragma once

#include "graphics/ui/ILeafMeasurer.hpp"
#include "graphics/ui/LayoutTypes.hpp"

namespace Engine {

struct LayoutInput
{
    LayoutConfig config;
    ILeafMeasurer* measurer = nullptr;
    uint parent = NO_NODE;
    uint sourceIndex = 0;
};

struct LayoutNode
{
    uint parent = NO_NODE;
    uint firstChild = NO_NODE;
    uint lastChild = NO_NODE;
    uint nextSibling = NO_NODE;
    uint childCount = 0;
    uint sourceIndex = 0;
    uint depth = 0;

    Vec2 pos = VEC2_ZERO;
    Vec2 size = VEC2_ZERO;
    Vec2 contentMin = VEC2_ZERO;
    Vec2 contentMax = VEC2_ZERO;

    uint lineStart = 0;
    uint lineCount = 0;

    bool isFloating = false;
    bool clipX = false;
    bool clipY = false;
};

}   // namespace Engine
