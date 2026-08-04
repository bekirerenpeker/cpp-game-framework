#pragma once

#include "graphics/ui/UINode.hpp"
#include "graphics/ui/leaf_types/UITextLeafData.hpp"
#include "utils/IdIndexedVector.hpp"
#include "utils/Singleton.hpp"
#include <string_view>
#include <vector>

namespace Engine {

// isHovered bubbles DOM-style to every ancestor of the node under the cursor;
// isHoveredDirectly is just the topmost one. isActive is the drag flag: it holds
// through release wherever the cursor goes, unlike isHeld, which dies the moment the
// cursor leaves the widget -- a slider reads isActive for exactly that reason.
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

    const Font* m_font = nullptr;

  public:
    void clear();

    void setFont(const Font* font) { m_font = font; }
    const Font* getFont() const { return m_font; }

    UINodeState openContainer(
        const UILayoutConfig& layout = {}, const UIContainerStyleSpec& style = {},
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

    UINodeState addNode(const UILayoutConfig& layout, std::string_view key = {});
};

}   // namespace Engine
