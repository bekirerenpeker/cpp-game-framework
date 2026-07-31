#pragma once

#include "graphics/ui/UIContainerStyle.hpp"
#include "leaf_types/IUILeafData.hpp"
#include "utils/IdIndexedVector.hpp"
#include "utils/TypeAliases.hpp"

namespace Engine {

struct UINode : IHasId
{
    IdType parent = INVALID_ID;
    IdType firstChild = INVALID_ID;
    IdType lastChild = INVALID_ID;
    IdType nextSibling = INVALID_ID;
    uint childCount = 0;

    UIContainerStyle style;
    IUILeafData* leafData = nullptr;
    bool isVisible = true;

    bool isContainer() const { return leafData == nullptr; }

    UILeafWidths measureWidths() const;
    float measureHeight(float contentWidth) const;

    void draw(Vec2 pos, Vec2 size) const;
};

}   // namespace Engine
