#pragma once

#include "graphics/ui/LayoutNode.hpp"
#include <vector>

namespace Engine {

class LayoutComputer
{
  private:
    static constexpr float EPSILON = 0.0001f;

    std::vector<LayoutInput> m_inputs;
    std::vector<LayoutNode> m_nodes;
    std::vector<LayoutLine> m_lines;
    std::vector<uint> m_scratch;
    Vec2 m_rootSize = VEC2_ZERO;
    bool m_warnedBadParent = false;

  public:
    void reset(size_t expectedNodeCount = 0);
    uint addNode(const LayoutInput& input);
    const std::vector<LayoutNode>& compute(Vec2 rootSize);

    const std::vector<LayoutNode>& getNodes() const { return m_nodes; }
    const std::vector<LayoutLine>& getLines() const { return m_lines; }
    const LayoutConfig& getConfig(uint index) const { return m_inputs[index].config; }
    size_t getNodeCount() const { return m_nodes.size(); }

  private:
    void computeIntrinsicWidths();
    void computeFinalWidths();
    void computeIntrinsicHeights();
    void computeFinalHeights();
    void computePositions();

    void aggregateIntrinsic(uint index, LayoutAxis axis);
    void finalizeIntrinsic(uint index, LayoutAxis axis);
    void distributeChildren(uint index, LayoutAxis axis, bool allowShrink);
    void resolveCrossAxis(uint index, LayoutAxis axis);
    float resolveChildAgainst(uint index, LayoutAxis axis, float inner) const;
    void positionChildren(uint index);
    void positionFloatingChild(uint parentIndex, uint childIndex);

    void levelUp(LayoutAxis axis, float remaining);
    void levelDown(LayoutAxis axis, float deficit);

    float seedChildSize(uint index, LayoutAxis axis, float available) const;
    float clampToSpec(uint index, LayoutAxis axis, float value) const;
    float contentFloor(uint index, LayoutAxis axis) const;
    float upperBound(uint index, LayoutAxis axis) const;
    uint layoutChildCount(uint index) const;
    float gapTotal(uint index) const;
    static float alignOffset(LayoutAlign align, float free);
};

}   // namespace Engine
