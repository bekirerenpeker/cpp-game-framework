#pragma once

#include "graphics/ui/UINode.hpp"
#include "utils/Singleton.hpp"
#include <vector>

namespace Engine {

constexpr uint NO_LAYOUT_NODE = (uint)-1;

// A flat mirror of one UINode for the duration of a solve. minWidth/maxWidth are
// scratch for the intrinsic passes; pos/size are the solved geometry in layout
// space, which is top-left anchored and Y-down. drawPos is display-only: the centre
// of the rect in a Y-up space, produced once every other pass has settled.
// There is no minHeight/maxHeight because nothing wraps vertically, so a node's
// intrinsic height is a single number -- it lives in size.y until the final pass
// overwrites it, which is safe only because that pass runs strictly top-down.
struct UILayoutNode
{
    const UINode* node = nullptr;
    uint64_t persistentKey = 0;
    // Copied off the UINode rather than read through it, because the snapshot kept
    // for next frame's hit test drops the pointer -- anything the hit test needs has
    // to live here. A leaf hands back no state, so hitting one would silently swallow
    // the hover its container should have had.
    bool acceptsInput = false;
    // Paint order is the preorder walk, so the tree already says what is above what.
    // This is the escape hatch for when that is not enough: a layer raises a node and
    // its whole subtree above everything in lower layers, whatever the walk says.
    uint paintLayer = 0;

    uint parent = NO_LAYOUT_NODE;
    uint firstChild = NO_LAYOUT_NODE;
    uint lastChild = NO_LAYOUT_NODE;
    uint nextSibling = NO_LAYOUT_NODE;
    uint childCount = 0;

    float minWidth = 0.0f;
    float maxWidth = 0.0f;

    Vec2 pos = VEC2_ZERO;
    Vec2 size = VEC2_ZERO;
    Vec2 drawPos = VEC2_ZERO;
};

class UILayoutCalculator : public Singleton<UILayoutCalculator>
{
    friend class Singleton<UILayoutCalculator>;

  private:
    static constexpr float EPSILON = 0.0001f;

    std::vector<UILayoutNode> m_prevFrameNodes, m_nodes;
    std::vector<uint> m_scratch;

  public:
    const std::vector<UILayoutNode>& calculate(IdType rootId, Vec2 rootTopLeft = VEC2_ZERO);
    const std::vector<UILayoutNode>& getNodes() const { return m_nodes; }
    const std::vector<UILayoutNode>& getPrevFrameNodes() const { return m_prevFrameNodes; }

    const UILayoutNode* getPrevFrameLayout(uint64_t key) const;
    void beginFrame() { m_prevFrameNodes.clear(); }

  private:
    UILayoutCalculator() = default;
    ~UILayoutCalculator() = default;

    uint buildSubtree(IdType nodeId, uint parentIndex);

    void computeIntrinsicWidths();
    void computeFinalWidths();
    void computeIntrinsicHeights();
    void computeFinalHeights();
    void computePositions();
    void applyTransforms();
    void computeDrawPositions(Vec2 rootTopLeft);

    void aggregateIntrinsic(uint index, UILayoutAxis axis);
    void finalizeIntrinsic(uint index, UILayoutAxis axis);
    void distributeChildren(uint index, UILayoutAxis axis, bool allowShrink);
    void resolveCrossAxis(uint index, UILayoutAxis axis);
    float resolveChildAgainst(uint index, UILayoutAxis axis, float inner) const;
    void positionChildren(uint index);
    void positionFloatingChild(uint parentIndex, uint childIndex);

    void levelUp(UILayoutAxis axis, float remaining);
    void levelDown(UILayoutAxis axis, float deficit);

    float resolveRootSize(UILayoutAxis axis) const;
    float seedChildSize(uint index, UILayoutAxis axis, float available) const;
    float clampToSpec(uint index, UILayoutAxis axis, float value) const;
    float contentFloor(uint index, UILayoutAxis axis) const;
    float upperBound(uint index, UILayoutAxis axis) const;
    uint layoutChildCount(uint index) const;
    float gapTotal(uint index) const;
    float marginTotal(uint index, UILayoutAxis axis) const;
    static float alignOffset(UIAlign align, float free);
};

}   // namespace Engine
