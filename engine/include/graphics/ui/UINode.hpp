#pragma once

#include "graphics/ui/UIContainerStyle.hpp"
#include "graphics/ui/UILayoutConfig.hpp"
#include "leaf_types/IUILeafData.hpp"
#include "utils/IdIndexedVector.hpp"
#include "utils/TypeAliases.hpp"

namespace Engine {

struct UILayoutNode;

struct UINode : IHasId
{
    IdType parent = INVALID_ID;
    IdType firstChild = INVALID_ID;
    IdType lastChild = INVALID_ID;
    IdType nextSibling = INVALID_ID;
    uint childCount = 0;

    UILayoutConfig layout;
    UIContainerStyle style;
    IUILeafData* leafData = nullptr;
    bool isVisible = true;

    uint64_t persistentKey = 0;
    const UILayoutNode* prevFrameLayout = nullptr;

    bool isContainer() const { return leafData == nullptr; }

    UILeafWidths measureWidths() const;
    float measureHeight(float contentWidth) const;

    void draw(Vec2 drawPos, Vec2 size) const;
};

}   // namespace Engine
