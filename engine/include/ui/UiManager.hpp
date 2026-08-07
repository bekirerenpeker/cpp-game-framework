#pragma once

#include "UINode.hpp"
#include "UIScrollbar.hpp"
#include "leafs/UIShaderLeafData.hpp"
#include "leafs/UITextLeafData.hpp"
#include "utils/IdIndexedVector.hpp"
#include "utils/Singleton.hpp"
#include <string_view>
#include <vector>

namespace Engine {

// The mouse and the node's own rect as they were on the frame the press landed. Held
// together because a drag needs both: the rect alone cannot say where inside it the grab
// was, and the mouse alone cannot say what the box was doing at the time.
struct UIDragOrigin
{
    Vec2 mousePos = VEC2_ZERO;
    Vec2 pos = VEC2_ZERO;
    Vec2 size = VEC2_ZERO;
};

// isHovered bubbles DOM-style to every ancestor of the node under the cursor;
// isHoveredDirectly is just the topmost one. isActive is the drag flag: it holds
// through release wherever the cursor goes, unlike isHeld, which dies the moment the
// cursor leaves the widget -- a slider reads isActive for exactly that reason.
// isPressed/isReleased/isHeld bubble too, but only as far as the first ancestor with
// style.blockInput set, which is what stops a button's click also firing the row it
// sits in. Hover deliberately keeps bubbling past it, the way CSS :hover does.
// Everything positional here is *solved* geometry in real pixels, so anything fed back
// into a config field has to go through UIWidgets::unscale or it scales twice.
struct UINodeState
{
    IdType id = INVALID_ID;
    bool isHovered = false;
    bool isHoveredDirectly = false;
    bool isActive = false;
    bool isPressed = false;
    bool isReleased = false;
    bool isHeld = false;
    bool isDoubleClicked = false;
    // Unlike everything above it, this one outlives the gesture that set it: focus is
    // where the keyboard goes, so it stays put while the mouse wanders anywhere else.
    // Only a node whose style says focusable can ever hold it.
    bool isFocused = false;
    Vec2 relativeMousePos = VEC2_ZERO;
    Vec2 localMousePos = VEC2_ZERO;
    Vec2 pos = VEC2_ZERO;
    Vec2 size = VEC2_ZERO;
    Vec2 mousePos = VEC2_ZERO;
    // Measured from the press, never accumulated frame to frame: adding up per-frame
    // deltas drifts, and drifts worst on the thing being dragged because it moves under
    // its own answer. Zero unless this node is the captured one.
    Vec2 dragDelta = VEC2_ZERO;
    UIDragOrigin pressOrigin;
    Vec2 grabOffset = VEC2_ZERO;
    Vec2 scrollDelta = VEC2_ZERO;
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
    // How far into that chain a press reaches: everything up to and including the first
    // node that blocks input. The whole chain when nothing blocks, which is why the
    // default costs nothing.
    size_t m_pressKeyCount = 0;
    uint64_t m_activeKey = NO_KEY;
    // Survives the press that set it, and the tree being rebuilt, because it is a
    // persistent key rather than a frame-local id -- that is the whole difference
    // between focus and the press capture above.
    uint64_t m_focusedKey = NO_KEY;
    // Reused by the tab walk so cycling does not allocate every keystroke.
    std::vector<uint64_t> m_scratchKeys;

    // Only one node is ever captured, so the press-time data is a handful of members
    // rather than an entry per node in the state store.
    UIDragOrigin m_pressOrigin;
    Vec2 m_pressGrabOffset = VEC2_ZERO;
    uint64_t m_lastClickKey = NO_KEY;
    float m_lastClickTime = 0.0f;
    bool m_isDoubleClick = false;

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

    // Hover alone is not enough: a drag that left the widget still owns the mouse.
    bool isMouseUsed() const
    {
        return !m_hoveredKeys.empty() || m_activeKey != NO_KEY || m_scrollDrag.key != NO_KEY;
    }
    bool isKeyboardUsed() const { return m_focusedKey != NO_KEY; }

    // Keyboard focus by persistent key, so a widget can hand it to a node it is about to
    // declare -- a field that should open focused, a dialog stealing it on appearance.
    uint64_t getFocusedKey() const { return m_focusedKey; }
    void setFocus(uint64_t key) { m_focusedKey = key; }
    void clearFocus() { m_focusedKey = NO_KEY; }

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
    void resolveFocus(const std::vector<UILayoutNode>& prev, uint hit);
    void cycleFocus(const std::vector<UILayoutNode>& prev, bool backwards);
    bool resolveScrollbars(const std::vector<UILayoutNode>& prev, Vec2 mouse);
    void routeScrollWheel(const std::vector<UILayoutNode>& prev, uint hit);
    void applyCursor(const std::vector<UILayoutNode>& prev, uint hit) const;
    void capturePress(const UILayoutNode& node, Vec2 mouse);
    UINodeState computeState(IdType id, uint64_t key, const UILayoutNode* prev) const;
    bool isKeyHovered(uint64_t key) const;
    bool isKeyPressable(uint64_t key) const;

    UINodeState addNode(const UILayoutConfig& layout, std::string_view key = {});
    void destroyNode(IdType id);
};

}   // namespace Engine
