#pragma once

#include "graphics/ui/UINode.hpp"
#include "graphics/ui/UIScrollbar.hpp"
#include "graphics/ui/leafs/UIShaderLeafData.hpp"
#include "graphics/ui/leafs/UITextLeafData.hpp"
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
    Vec2 pos = VEC2_ZERO;
    Vec2 size = VEC2_ZERO;
    // The node's identity across frames, which the frame-local id is not: this is what
    // addresses its entry in UIStateStore.
    uint64_t persistentKey = 0;
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

    // A bar is not a node, so it cannot ride on m_activeKey: it carries its own capture,
    // and the origins with it, the same shape one node's drag would have taken.
    struct ScrollDrag
    {
        uint64_t key = NO_KEY;
        UILayoutAxis axis = UILayoutAxis::Vertical;
        float pointerOrigin = 0.0f;
        float scrollOrigin = 0.0f;
    };

    ScrollDrag m_scrollDrag;
    uint64_t m_scrollHoverKey = NO_KEY;
    UILayoutAxis m_scrollHoverAxis = UILayoutAxis::Vertical;
    UIScrollbarStyle m_scrollbarStyle;

    const Font* m_font = nullptr;

  public:
    void clear();

    void setFont(const Font* font) { m_font = font; }
    const Font* getFont() const { return m_font; }

    // Pushed in rather than read from the theme, the same way the font is: a bar belongs
    // to no container, so there is no style spec it could have come from.
    void setScrollbarStyle(const UIScrollbarStyle& style) { m_scrollbarStyle = style; }
    const UIScrollbarStyle& getScrollbarStyle() const { return m_scrollbarStyle; }

    bool isMouseOverUi() const { return !m_hoveredKeys.empty(); }

    UINodeState openContainer(
        const UILayoutConfig& layout = {}, const UIContainerStyleSpec& style = {},
        std::string_view key = {}
    );
    void closeContainer();

    IdType addTextLeaf(const UILayoutConfig& layout, const UITextConfig& config);
    IdType addShaderLeaf(const UILayoutConfig& layout, const UIShaderConfig& config);

    // Discards a subtree that was already declared this frame. Immediate mode gives a
    // widget no way to stop its caller declaring content, so a collapsed window throws
    // the content away afterwards instead.
    void removeChildren(IdType id);

    void draw();

    UINode* getNode(IdType id) { return m_nodes.get(id); }
    const UINode* getNode(IdType id) const { return m_nodes.get(id); }
    const IdIndexedVector<UINode>& getNodes() const { return m_nodes; }

  private:
    UIManager() = default;
    ~UIManager();

    void resolveInput();
    bool resolveScrollbars(const std::vector<UILayoutNode>& prev, Vec2 mouse);
    void routeScrollWheel(const std::vector<UILayoutNode>& prev, uint hit);
    bool isKeyHovered(uint64_t key) const;

    UINodeState addNode(const UILayoutConfig& layout, std::string_view key = {});
    void destroyNode(IdType id);
};

}   // namespace Engine
