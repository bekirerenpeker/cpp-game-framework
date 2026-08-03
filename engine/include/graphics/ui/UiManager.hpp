#pragma once

#include "graphics/ui/UINode.hpp"
#include "graphics/ui/leaf_types/UITextLeafData.hpp"
#include "utils/IdIndexedVector.hpp"
#include "utils/Singleton.hpp"
#include <string_view>
#include <vector>

namespace Engine {

// isHovered follows the DOM rule: the node under the cursor and every ancestor of it,
// so a panel still counts as hovered while the cursor sits on one of its children.
// isHoveredDirectly is the single topmost node, for a widget that has to know the
// cursor is on *it* rather than somewhere inside it.
//
// isActive is the drag flag: this node was pressed and holds the mouse until release
// **wherever the cursor goes**, which is what a slider reads instead of isHeld --
// isHeld rides on hover and so dies the moment the cursor leaves the track, which is
// most of a real drag. While one node is active no other node can be hovered at all.
struct UINodeState
{
    IdType id = INVALID_ID;
    bool isHovered = false;
    bool isHoveredDirectly = false;
    bool isActive = false;
    bool isPressed = false;
    bool isReleased = false;
    bool isHeld = false;
    Vec2 relativeMousePos = VEC2_ZERO;
    Vec2 localMousePos = VEC2_ZERO;
};

class UIManager : public Singleton<UIManager>
{
    friend class Singleton<UIManager>;

  private:
    IdIndexedVector<UINode> m_nodes;
    std::vector<IUILeafData*> m_leaves;
    std::vector<IdType> m_openStack;
    std::vector<IdType> m_roots;
    static constexpr uint64_t NO_KEY = 0;

    // The hit node followed by its ancestors, innermost first. Empty when nothing is
    // under the cursor; a handful of entries otherwise, so a scan beats a set.
    std::vector<uint64_t> m_hoveredKeys;
    uint64_t m_activeKey = NO_KEY;

  public:
    void clear();

    UINodeState openContainer(
        const UILayoutConfig& layout = {}, const UIContainerStyle& style = {},
        std::string_view key = {}
    );
    void closeContainer();

    IdType addTextLeaf(const UILayoutConfig& layout, const UITextConfig& config);

    void draw();

    UINode* getNode(IdType id) { return m_nodes.get(id); }
    const UINode* getNode(IdType id) const { return m_nodes.get(id); }
    const IdIndexedVector<UINode>& getNodes() const { return m_nodes; }

  private:
    UIManager() = default;
    ~UIManager();

    void resolveInput();
    bool isKeyHovered(uint64_t key) const;

    UINodeState
    addNode(const UILayoutConfig& layout, const UIContainerStyle& style, std::string_view key = {});
};

}   // namespace Engine
