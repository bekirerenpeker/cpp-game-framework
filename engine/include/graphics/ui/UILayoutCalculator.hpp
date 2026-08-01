#pragma once

#include "graphics/ui/UINode.hpp"
#include "utils/Singleton.hpp"
#include <vector>

namespace Engine {

constexpr uint NO_LAYOUT_NODE = (uint)-1;

// A flat mirror of one UINode for the duration of a solve. minWidth/maxWidth are
// scratch for the intrinsic passes; width/height/x/y are the solved geometry. There
// is no minHeight/maxHeight because nothing wraps vertically, so a node's intrinsic
// height is a single number -- it is stored in height and overwritten by the final
// pass, which is safe only because that pass runs strictly top-down.
struct UILayoutNode
{
    const UINode* node = nullptr;

    uint parent = NO_LAYOUT_NODE;
    uint firstChild = NO_LAYOUT_NODE;
    uint lastChild = NO_LAYOUT_NODE;
    uint nextSibling = NO_LAYOUT_NODE;
    uint childCount = 0;

    float minWidth = 0.0f;
    float maxWidth = 0.0f;

    float width = 0.0f;
    float height = 0.0f;
    float x = 0.0f;
    float y = 0.0f;
};

class UILayoutCalculator : public Singleton<UILayoutCalculator>
{
    friend class Singleton<UILayoutCalculator>;

  private:
    static constexpr float EPSILON = 0.0001f;

    std::vector<UILayoutNode> m_nodes;
    std::vector<uint> m_scratch;
    Vec2 m_rootSize = VEC2_ZERO;

  public:
    const std::vector<UILayoutNode>& calculate(IdType rootId, Vec2 rootSize);
    const std::vector<UILayoutNode>& getNodes() const { return m_nodes; }

  private:
    UILayoutCalculator() = default;
    ~UILayoutCalculator() = default;

    uint buildSubtree(IdType nodeId, uint parentIndex);

    void computeIntrinsicWidths();
    void computeFinalWidths();
    void computeIntrinsicHeights();
    void computeFinalHeights();
    void computePositions();

    void aggregateIntrinsic(uint index, UILayoutAxis axis);
    void finalizeIntrinsic(uint index, UILayoutAxis axis);
    void distributeChildren(uint index, UILayoutAxis axis, bool allowShrink);
    void resolveCrossAxis(uint index, UILayoutAxis axis);
    float resolveChildAgainst(uint index, UILayoutAxis axis, float inner) const;
    void positionChildren(uint index);
    void positionFloatingChild(uint parentIndex, uint childIndex);

    void levelUp(UILayoutAxis axis, float remaining);
    void levelDown(UILayoutAxis axis, float deficit);

    float seedChildSize(uint index, UILayoutAxis axis, float available) const;
    float clampToSpec(uint index, UILayoutAxis axis, float value) const;
    float contentFloor(uint index, UILayoutAxis axis) const;
    float upperBound(uint index, UILayoutAxis axis) const;
    uint layoutChildCount(uint index) const;
    float gapTotal(uint index) const;
    static float alignOffset(UIAlign align, float free);
};

}   // namespace Engine
