#include "ui/UiManager.hpp"
#include "context/GlfwContext.hpp"
#include "core/Time.hpp"
#include "core/input/Input.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "core/window_management/ViewContext.hpp"
#include "core/window_management/Window.hpp"
#include "graphics/text/TextRenderer.hpp"
#include "ui/UILayoutCalculator.hpp"
#include "ui/UIRenderer.hpp"
#include "ui/UIStateStore.hpp"
#include "ui/theme/UIThemeManager.hpp"
#include "utils/math/MathFuncs.hpp"
#include <algorithm>
#include <functional>

namespace Engine {

namespace {

// Pixels per wheel tick, scaled with the UI so a notch travels the same apparent
// distance whatever the theme scale is.
constexpr float SCROLL_WHEEL_STEP = 48.0f;

constexpr float DOUBLE_CLICK_SECONDS = 0.4f;

// Distinct seeds so an unnamed node's positional index and a named node's hash never
// collide into the same key.
constexpr uint64_t POSITIONAL_KEY_SEED = 1;
constexpr uint64_t NAMED_KEY_SEED = 2;

uint64_t localKeyOf(std::string_view key, uint64_t positionalIndex)
{
    return key.empty() ? uiHashCombine(POSITIONAL_KEY_SEED, positionalIndex) :
                         uiHashCombine(NAMED_KEY_SEED, std::hash<std::string_view> {}(key));
}

// A node is hittable only where it is actually visible, so the accumulated clip has to
// be part of the test -- otherwise the half of a list item scrolled out of its
// container still swallows clicks, which is worse than the overdraw it replaced.
// The painter walks roots in order, then layers inside a root, then preorder inside a
// layer. Hit testing has to break ties the same way or a node that visibly covers
// another can still lose the click to it.
bool paintsAbove(const UILayoutNode& node, const UILayoutNode& other)
{
    if (node.rootOrder != other.rootOrder) return node.rootOrder > other.rootOrder;
    if (node.paintLayer != other.paintLayer) return node.paintLayer > other.paintLayer;
    return true;
}

bool containsClip(const Vec4& clip, Vec2 mouse)
{
    return mouse.x >= clip.x && mouse.y >= clip.y && mouse.x <= clip.z && mouse.y <= clip.w;
}

bool containsMouse(const UILayoutNode& node, Vec2 mouse)
{
    Vec2 half = node.size * 0.5f;
    Vec2 delta = mouse - node.drawPos;
    if (Math::abs(delta.x) > half.x || Math::abs(delta.y) > half.y) return false;

    return containsClip(node.clipRect, mouse);
}

CursorShape cursorShapeOf(UICursor cursor)
{
    switch (cursor) {
    case UICursor::Pointer   : return CursorShape::Hand;
    case UICursor::Text      : return CursorShape::IBeam;
    case UICursor::Move      : return CursorShape::ResizeAll;
    case UICursor::Crosshair : return CursorShape::Crosshair;
    case UICursor::NotAllowed: return CursorShape::NotAllowed;
    case UICursor::ResizeEW  : return CursorShape::ResizeEW;
    case UICursor::ResizeNS  : return CursorShape::ResizeNS;
    case UICursor::ResizeNWSE: return CursorShape::ResizeNWSE;
    case UICursor::ResizeNESW: return CursorShape::ResizeNESW;
    default                  : return CursorShape::Arrow;
    }
}

void scaleSize(UISizeSpec& spec, float scale)
{
    if (spec.mode == UISizeMode::Fixed) spec.value *= scale;
    spec.min *= scale;
    // UI_UNBOUNDED is a sentinel, not a length; scaling it would still read as unbounded
    // but stops comparing equal to the constant every other site tests against.
    if (spec.max < UI_UNBOUNDED) spec.max *= scale;
}

void scaleEdges(UIEdges& edges, float scale)
{
    edges.left *= scale;
    edges.right *= scale;
    edges.top *= scale;
    edges.bottom *= scale;
}

// Every pixel-valued layout field, multiplied once as the node is created. Applying the
// scale to the *inputs* rather than to the projection is what keeps the solve's output in
// real window pixels, so hit testing, clip rects and the mouse all keep working untouched.
// Percent and Grow are ratios and are deliberately left alone.
void applyScale(UILayoutConfig& layout, float scale)
{
    scaleSize(*layout.width, scale);
    scaleSize(*layout.height, scale);
    scaleEdges(*layout.padding, scale);
    scaleEdges(*layout.margin, scale);

    layout.floating->offset = layout.floating->offset * scale;
    *layout.gap *= scale;
    *layout.offset = *layout.offset * scale;

    for (UISizeSpec& track : layout.grid->columns) scaleSize(track, scale);
    // Negative is the "same as gap" sentinel, and gap is scaled on its own line above.
    if (layout.grid->rowGap > 0.0f) layout.grid->rowGap *= scale;
}

// The style's own lengths, or a scaled-up UI keeps hairline borders and tight radii on
// boxes twice the size. None of these feeds hit testing, so scaling them here is safe.
void applyScale(UIContainerStyle& style, float scale)
{
    *style.borderWidth *= scale;

    UICorners& radius = *style.borderRadius;
    radius.topLeft *= scale;
    radius.topRight *= scale;
    radius.bottomRight *= scale;
    radius.bottomLeft *= scale;

    *style.shadowOffset = *style.shadowOffset * scale;
    *style.shadowBlurRadius *= scale;
}

// Lowest priority first, so a pressed button keeps the rest of its hover look instead
// of falling back to the base. onHeld reads isActive rather than isHeld so a grip
// dragged off itself stays lit until release, since isHeld dies the moment hover does.
// onFocused sits above hover and below the press states: focus outlasts both, so it must
// not paint over the feedback of a click happening right now.
UIContainerStyle
resolveStyle(const UIContainerStyleSpec& spec, const UINodeState& state, float scale)
{
    UIContainerStyle style = spec.base();
    if (state.isHovered) style.combine(spec.onHover);
    if (state.isFocused) style.combine(spec.onFocused);
    if (state.isActive) style.combine(spec.onHeld);
    if (state.isPressed) style.combine(spec.onPressed);
    if (state.isReleased) style.combine(spec.onReleased);

    // Filled once the states have all merged, so every field is engaged by the time the
    // node is stored -- the renderer then reads plain values instead of re-deciding a
    // default per field, and the defaults themselves live only in UIContainerStyle.
    style.fillDefaults();
    if (scale != 1.0f) applyScale(style, scale);
    return style;
}

}   // namespace

UIManager::~UIManager() { clear(); }

void UIManager::clear()
{
    for (IUILeafData* leaf : m_leaves) delete leaf;
    m_leaves.clear();
    m_nodes.clear();
    m_openStack.clear();
    m_roots.clear();

    UIStateStore::get().beginFrame();

    // Resolved here rather than at the end of draw() because this is the last moment
    // before the tree is rebuilt, so it pairs this frame's mouse with last frame's
    // geometry -- and every openContainer after it needs the answer already settled.
    resolveInput();
}

// Exactly one node is hovered: the last hit in paint order (children after parents,
// later roots after earlier ones), which is topmost. Resolution has to mirror the
// painter's own walk, so honouring zIndex means reordering both together.
void UIManager::resolveInput()
{
    m_hoveredKeys.clear();
    m_pressKeyCount = 0;
    m_isDoubleClick = false;

    Vec2 mouse = UIRenderer::get().getMouseUiPos();
    const std::vector<UILayoutNode>& prev = UILayoutCalculator::get().getPrevFrameNodes();
    Input& input = Input::get();

    // Scrollbars are derived from solved geometry rather than declared as nodes, so they
    // are not in the array everything below searches and have to be resolved first.
    // Taking the frame when one is touched is not a shortcut: a bar paints above every
    // node in its layer, so the content visually underneath it must not answer instead.
    if (resolveScrollbars(prev, mouse)) {
        m_activeKey = NO_KEY;
        applyCursor(prev, NO_LAYOUT_NODE);
        return;
    }

    // Capture survives one frame past the button coming up, so the node that was
    // pressed is the one that sees isReleased. Dropping it on button-up would hand
    // the release to whatever the cursor had wandered onto by then.
    if (!input.mouseButtonHeld(MouseButton::Left) && !input.mouseButtonReleased(MouseButton::Left))
        m_activeKey = NO_KEY;

    uint active = NO_LAYOUT_NODE;
    if (m_activeKey != NO_KEY) {
        for (uint i = 0; i < (uint)prev.size(); i++) {
            if (prev[i].persistentKey != m_activeKey) continue;
            active = i;
            break;
        }
        // The captured node stopped being declared, so nothing will ever match the
        // key again -- drop it rather than deadlocking input on a ghost.
        if (active == NO_LAYOUT_NODE) m_activeKey = NO_KEY;
    }

    uint hit = NO_LAYOUT_NODE;
    if (m_activeKey != NO_KEY) {
        // Nothing else may be hovered while a drag is live -- that is the whole point
        // of capture: dragging a slider past a button must not light the button. The
        // captured node still only counts as *hovered* while the cursor is genuinely
        // inside it, so pressing it, dragging away and releasing does not fire it.
        if (containsMouse(prev[active], mouse)) hit = active;
    } else {
        for (uint i = 0; i < (uint)prev.size(); i++) {
            if (!prev[i].acceptsInput) continue;
            if (!containsMouse(prev[i], mouse)) continue;
            if (hit != NO_LAYOUT_NODE && !paintsAbove(prev[i], prev[hit])) continue;
            hit = i;
        }
        if (hit != NO_LAYOUT_NODE && input.mouseButtonPressed(MouseButton::Left)) {
            m_activeKey = prev[hit].persistentKey;
            capturePress(prev[hit], mouse);
        }
    }

    resolveFocus(prev, hit);

    if (hit == NO_LAYOUT_NODE) {
        applyCursor(prev, hit);
        return;
    }

    // The hit node first, then every ancestor. Walking up is what keeps a panel lit
    // while the cursor is on one of its own children; stopping at the hit node alone
    // reads as the panel flickering off whenever the cursor crosses its contents.
    // The press chain is the same walk cut short at the first node that claims the
    // click, so hover reaches the whole panel while the press stops at the button.
    for (uint i = hit; i != NO_LAYOUT_NODE; i = prev[i].parent) {
        m_hoveredKeys.push_back(prev[i].persistentKey);
        if (m_pressKeyCount == 0 && prev[i].blocksInput) m_pressKeyCount = m_hoveredKeys.size();
    }
    if (m_pressKeyCount == 0) m_pressKeyCount = m_hoveredKeys.size();

    applyCursor(prev, hit);
    routeScrollWheel(prev, hit);
}

// Focus follows the press, not the cursor: it is claimed on mouse-down and then held
// until something takes it, which is what lets a field keep the keyboard while the mouse
// is off doing something else. A press on a non-focusable node still clears it, because
// clicking elsewhere is how a user says "I'm done with that field".
void UIManager::resolveFocus(const std::vector<UILayoutNode>& prev, uint hit)
{
    // The node holding focus stopped being declared -- drop it rather than keeping the
    // keyboard pointed at a key nothing will ever match again, exactly as capture does.
    if (m_focusedKey != NO_KEY) {
        bool stillDeclared = false;
        for (const UILayoutNode& node : prev) {
            if (node.persistentKey != m_focusedKey) continue;
            stillDeclared = node.isFocusable;
            break;
        }
        if (!stillDeclared) m_focusedKey = NO_KEY;
    }

    Input& input = Input::get();
    if (input.keyRepeated(KeyCode::Tab)) {
        bool backwards = input.keyHeld(KeyCode::LeftShift) || input.keyHeld(KeyCode::RightShift);
        cycleFocus(prev, backwards);
        return;
    }

    if (!input.mouseButtonPressed(MouseButton::Left)) return;

    // Outward from the hit node, so clicking a label or an icon inside a field focuses
    // the field rather than nothing -- the same walk hover and the press chain use.
    for (uint i = hit; i != NO_LAYOUT_NODE; i = prev[i].parent) {
        if (!prev[i].isFocusable) continue;
        m_focusedKey = prev[i].persistentKey;
        return;
    }
    m_focusedKey = NO_KEY;
}

// Declaration order is tab order: the layout array is preorder, so it already holds the
// nodes in the order the caller wrote them. Wraps at both ends rather than stopping, and
// starting from nothing focuses the first (or last, going backwards).
void UIManager::cycleFocus(const std::vector<UILayoutNode>& prev, bool backwards)
{
    m_scratchKeys.clear();
    for (const UILayoutNode& node : prev) {
        if (node.isFocusable) m_scratchKeys.push_back(node.persistentKey);
    }
    if (m_scratchKeys.empty()) return;

    size_t current = m_scratchKeys.size();
    for (size_t i = 0; i < m_scratchKeys.size(); i++) {
        if (m_scratchKeys[i] != m_focusedKey) continue;
        current = i;
        break;
    }

    size_t count = m_scratchKeys.size();
    size_t next;
    if (current == count) next = backwards ? count - 1 : 0;
    else next = backwards ? (current + count - 1) % count : (current + 1) % count;

    m_focusedKey = m_scratchKeys[next];
}

// The mouse and the node's rect frozen at the moment of the press, so every drag after
// it measures from one fixed point instead of summing per-frame deltas -- which drifts,
// and drifts worst on the node being dragged, since it moves under its own answer.
void UIManager::capturePress(const UILayoutNode& node, Vec2 mouse)
{
    Vec2 half = node.size * 0.5f;
    Vec2 centerDelta = mouse - node.drawPos;

    m_pressOrigin.mousePos = mouse;
    m_pressOrigin.pos = node.pos;
    m_pressOrigin.size = node.size;
    // Y-down and top-left anchored, matching localMousePos rather than the renderer's
    // centre-anchored Y-up draw space.
    m_pressGrabOffset = Vec2(centerDelta.x + half.x, half.y - centerDelta.y);

    float now = Time::get().currTime();
    m_isDoubleClick =
        node.persistentKey == m_lastClickKey && now - m_lastClickTime <= DOUBLE_CLICK_SECONDS;

    m_lastClickKey = node.persistentKey;
    // Reset rather than kept, or the third click of a triple reads as a second double.
    m_lastClickTime = m_isDoubleClick ? 0.0f : now;
}

// Walks outward from the node under the cursor and takes the first node that names a
// cursor, so UICursor::Default reads as "no opinion" and inherits from an ancestor --
// otherwise every container in the tree would fight over the shape every frame.
void UIManager::applyCursor(const std::vector<UILayoutNode>& prev, uint hit) const
{
    UICursor cursor = UICursor::Default;
    for (uint i = hit; i != NO_LAYOUT_NODE; i = prev[i].parent) {
        if (prev[i].cursor == UICursor::Default) continue;
        cursor = prev[i].cursor;
        break;
    }

    Window* window = ViewContext::get().getActiveWindow();
    if (window) window->setCursor(cursorShapeOf(cursor));
}

// Outward from the node under the cursor. An axis this node cannot move on is left in
// the wheel and carries on up, which is what makes a list inside a panel hand the rest
// of the gesture to the panel once it has reached its own end.
void UIManager::routeScrollWheel(const std::vector<UILayoutNode>& prev, uint hit)
{
    Vec2 wheel = Input::get().getScrollDelta();
    if (wheel == VEC2_ZERO) return;

    wheel = wheel * (SCROLL_WHEEL_STEP * UIThemeManager::get().getScale());
    UIStateStore& store = UIStateStore::get();

    for (uint i = hit; i != NO_LAYOUT_NODE; i = prev[i].parent) {
        if (wheel == VEC2_ZERO) return;
        if (!prev[i].isScrollable) continue;

        Vec2& scroll = store.systemState(prev[i].persistentKey).scroll;
        Vec2 range = Vec2(
            Math::max(prev[i].contentSize.x - prev[i].size.x, 0.0f),
            Math::max(prev[i].contentSize.y - prev[i].size.y, 0.0f)
        );

        // y is negated because a wheel reads y-up and a scroll offset is y-down.
        float x = Math::clamp(scroll.x + wheel.x, 0.0f, range.x);
        float y = Math::clamp(scroll.y - wheel.y, 0.0f, range.y);

        if (x != scroll.x) {
            scroll.x = x;
            wheel.x = 0.0f;
        }
        if (y != scroll.y) {
            scroll.y = y;
            wheel.y = 0.0f;
        }
    }
}

bool UIManager::resolveScrollbars(const std::vector<UILayoutNode>& prev, Vec2 mouse)
{
    Input& input = Input::get();
    UIStateStore& store = UIStateStore::get();
    float scale = UIThemeManager::get().getScale();

    m_scrollHoverKey = NO_KEY;
    if (!input.mouseButtonHeld(MouseButton::Left) && !input.mouseButtonReleased(MouseButton::Left))
        m_scrollDrag.key = NO_KEY;

    if (m_scrollDrag.key != NO_KEY) {
        for (const UILayoutNode& node : prev) {
            if (node.persistentKey != m_scrollDrag.key) continue;

            UIScrollbar bar = scrollbarOf(node, m_scrollDrag.axis, m_scrollbarStyle, scale);
            if (bar.travel <= 0.0f) break;

            // The pointer is y-up and the offset y-down, so the vertical drag reads the
            // opposite way round from the horizontal one.
            float moved = m_scrollDrag.axis == UILayoutAxis::Vertical ?
                              m_scrollDrag.pointerOrigin - mouse.y :
                              mouse.x - m_scrollDrag.pointerOrigin;

            float value = m_scrollDrag.scrollOrigin + moved / bar.travel * bar.range;
            Vec2& scroll = store.systemState(node.persistentKey).scroll;
            axisSet(scroll, m_scrollDrag.axis, Math::clamp(value, 0.0f, bar.range));

            m_scrollHoverKey = m_scrollDrag.key;
            m_scrollHoverAxis = m_scrollDrag.axis;
            break;
        }
        return true;
    }

    // A live node drag outranks a bar: dragging a slider across one must not hand the
    // gesture over halfway through.
    if (m_activeKey != NO_KEY) return false;

    const UILayoutNode* found = nullptr;
    UILayoutAxis foundAxis = UILayoutAxis::Vertical;

    for (const UILayoutNode& node : prev) {
        if (!node.isScrollable || !containsClip(node.clipRect, mouse)) continue;
        if (found && !paintsAbove(node, *found)) continue;

        for (UILayoutAxis axis : {UILayoutAxis::Vertical, UILayoutAxis::Horizontal}) {
            if (!scrollbarThumbContains(scrollbarOf(node, axis, m_scrollbarStyle, scale), mouse))
                continue;
            found = &node;
            foundAxis = axis;
        }
    }

    if (!found) return false;

    m_scrollHoverKey = found->persistentKey;
    m_scrollHoverAxis = foundAxis;

    if (input.mouseButtonPressed(MouseButton::Left)) {
        m_scrollDrag.key = found->persistentKey;
        m_scrollDrag.axis = foundAxis;
        m_scrollDrag.pointerOrigin = foundAxis == UILayoutAxis::Vertical ? mouse.y : mouse.x;
        m_scrollDrag.scrollOrigin = axisGet(found->scroll, foundAxis);
    }

    return true;
}

bool UIManager::isKeyHovered(uint64_t key) const
{
    for (uint64_t hovered : m_hoveredKeys)
        if (hovered == key) return true;
    return false;
}

bool UIManager::isKeyPressable(uint64_t key) const
{
    for (size_t i = 0; i < m_pressKeyCount; i++)
        if (m_hoveredKeys[i] == key) return true;
    return false;
}

UINodeState UIManager::computeState(IdType id, uint64_t key, const UILayoutNode* prev) const
{
    UINodeState state;
    state.id = id;
    state.persistentKey = key;
    state.isHovered = isKeyHovered(key);
    state.isHoveredDirectly = !m_hoveredKeys.empty() && m_hoveredKeys.front() == key;
    state.isActive = m_activeKey != NO_KEY && key == m_activeKey;
    state.isFocused = m_focusedKey != NO_KEY && key == m_focusedKey;
    state.mousePos = UIRenderer::get().getMouseUiPos();

    // Bounded by the press chain rather than by hover, so a click inside a child reaches
    // the containers around it only up to the first one that claims it. Two *overlapping*
    // containers still cannot both react, since only the topmost one's ancestors are in
    // the chain at all.
    Input& input = Input::get();
    bool pressable = isKeyPressable(key);
    state.isPressed = pressable && input.mouseButtonPressed(MouseButton::Left);
    state.isReleased = pressable && input.mouseButtonReleased(MouseButton::Left);
    state.isHeld = pressable && input.mouseButtonHeld(MouseButton::Left);
    state.isDoubleClicked = state.isPressed && m_isDoubleClick && key == m_lastClickKey;

    // The whole wheel, not what routeScrollWheel left of it: a widget reading this is
    // not a scroll container and has no offset of its own for the routing to consume.
    if (state.isHovered) state.scrollDelta = input.getScrollDelta();

    if (state.isActive) {
        state.pressOrigin = m_pressOrigin;
        state.grabOffset = m_pressGrabOffset;
        state.dragDelta = state.mousePos - m_pressOrigin.mousePos;
    }

    if (!prev) return state;

    Vec2 half = prev->size * 0.5f;
    // Both sides come from the same space -- the mouse through UIRenderer, drawPos
    // baked by the solver -- so screen and world hit test through one path. drawPos is
    // centre-anchored and Y-up; flip into the box's own top-left/Y-down space so
    // local/relative match the rest of the UI instead of the render convention.
    Vec2 centerDelta = state.mousePos - prev->drawPos;
    state.localMousePos = Vec2(centerDelta.x + half.x, half.y - centerDelta.y);
    state.relativeMousePos =
        prev->size.x > 0.0f && prev->size.y > 0.0f ? state.localMousePos / prev->size : VEC2_ZERO;
    state.pos = prev->pos;
    state.size = prev->size;
    return state;
}

UINodeState UIManager::addNode(const UILayoutConfig& layout, std::string_view key)
{
    IdType id = m_nodes.add();
    UINode* node = m_nodes.get(id);

    // Both filled here, at the single point every node passes through, so the solver and
    // the renderer can dereference straight through instead of each read site naming a
    // default of its own. The style matters for a *leaf* especially: openContainer is
    // what resolves a container's, and nothing else would ever fill this one, yet the
    // solver still reads zIndex and overflow off every node it walks.
    node->layout = layout;
    node->layout.fillDefaults();
    node->style.fillDefaults();

    // A container's real style is resolved and scaled in openContainer; this one only
    // ever survives on a leaf, whose zIndex and overflow the solver still reads.
    float scale = UIThemeManager::get().getScale();
    if (scale != 1.0f) {
        applyScale(node->layout, scale);
        applyScale(node->style, scale);
    }

    if (m_openStack.empty()) {
        node->parent = INVALID_ID;
        node->persistentKey = uiHashCombine(0, localKeyOf(key, (uint64_t)m_roots.size()));
        m_roots.push_back(id);
        return {.id = id, .persistentKey = node->persistentKey};
    }

    IdType parent = m_openStack.back();
    node->parent = parent;

    UINode* parentNode = m_nodes.get(parent);
    if (!parentNode) return {.id = id};

    node->persistentKey =
        uiHashCombine(parentNode->persistentKey, localKeyOf(key, (uint64_t)parentNode->childCount));

    if (parentNode->lastChild == INVALID_ID) parentNode->firstChild = id;
    else m_nodes.get(parentNode->lastChild)->nextSibling = id;

    parentNode->lastChild = id;
    parentNode->childCount++;

    return {.id = id, .persistentKey = node->persistentKey};
}

UINodeState UIManager::openContainer(
    const UILayoutConfig& layout, const UIContainerStyleSpec& style, std::string_view key
)
{
    UINodeState state = addNode(layout, key);
    UINode* node = m_nodes.get(state.id);
    if (node) {
        node->prevFrameLayout = UILayoutCalculator::get().getPrevFrameLayout(node->persistentKey);
        state = computeState(state.id, node->persistentKey, node->prevFrameLayout);

        // Flattened here rather than stored, so the node still carries one final style
        // and nothing downstream has to know a state ever existed. The caller can still
        // rewrite node->style afterwards for anything a constant cannot express.
        node->style = resolveStyle(style, state, UIThemeManager::get().getScale());
    }

    m_openStack.push_back(state.id);
    return state;
}

void UIManager::closeContainer()
{
    if (m_openStack.empty()) {
        LOG_ERROR(
            "closeContainer called with no container open; the open/close pairs are unbalanced"
        );
        return;
    }
    m_openStack.pop_back();
}

IdType UIManager::addTextLeaf(const UILayoutConfig& layout, const UITextConfig& config)
{
    // A leaf that clips or ellipsises has to be allowed to be narrower than its own
    // text, or it keeps its full width and there is nothing left to cut. This is the
    // same rule a container gets from style.overflow -- a leaf has no style to carry
    // one, so it comes off the text config instead and the caller sets one field.
    UILayoutConfig leafLayout = layout;
    if (config.overflow != TextOverflow::Visible) leafLayout.clipX = true;

    UINodeState state = addNode(leafLayout);
    UINode* node = m_nodes.get(state.id);
    if (!node) return state.id;

    UITextLeafData* leaf = new UITextLeafData(config, UIThemeManager::get().getScale());
    // Set on the block rather than on a copy of the config, which owns a spanStyles
    // vector this would have to copy once per text leaf per frame. Still null if the UI
    // was never given a font, and that is fine: the text path resolves null to
    // FontLoader's default.
    if (!config.font) leaf->getBlock().setFont(m_font);

    m_leaves.push_back(leaf);
    node->leafData = leaf;
    return state.id;
}

IdType UIManager::addShaderLeaf(const UILayoutConfig& layout, const UIShaderConfig& config)
{
    UINodeState state = addNode(layout);
    UINode* node = m_nodes.get(state.id);
    if (!node) return state.id;

    UIShaderLeafData* leaf = new UIShaderLeafData(config);
    m_leaves.push_back(leaf);
    node->leafData = leaf;
    return state.id;
}

void UIManager::removeChildren(IdType id)
{
    UINode* node = m_nodes.get(id);
    if (!node) return;

    for (IdType childId = node->firstChild; childId != INVALID_ID;) {
        const UINode* child = m_nodes.get(childId);
        if (!child) break;

        IdType next = child->nextSibling;
        destroyNode(childId);
        childId = next;
    }

    // Re-fetched: removing the children swap-removes entries, which moves the storage
    // this pointer was aimed at.
    node = m_nodes.get(id);
    if (!node) return;

    node->firstChild = INVALID_ID;
    node->lastChild = INVALID_ID;
    node->childCount = 0;
}

void UIManager::destroyNode(IdType id)
{
    const UINode* node = m_nodes.get(id);
    if (!node) return;

    for (IdType childId = node->firstChild; childId != INVALID_ID;) {
        const UINode* child = m_nodes.get(childId);
        if (!child) break;

        IdType next = child->nextSibling;
        destroyNode(childId);
        childId = next;
    }

    node = m_nodes.get(id);
    if (!node) return;

    if (node->leafData) {
        auto found = std::find(m_leaves.begin(), m_leaves.end(), node->leafData);
        if (found != m_leaves.end()) {
            delete *found;
            m_leaves.erase(found);
        }
    }
    m_nodes.remove(id);
}

void UIManager::draw()
{
    if (!m_openStack.empty()) {
        LOG_ERROR(
            "{} container(s) left open at draw; every openContainer needs a closeContainer",
            m_openStack.size()
        );
    }

    UIRenderer& renderer = UIRenderer::get();

    // Glyphs have to reach the same space as the boxes. Setting the override flushes,
    // which is what keeps any world text submitted earlier this frame on the world
    // matrix instead of retroactively landing on this one.
    if (renderer.getSpace() == UISpace::World) TextRenderer::get().clearViewProjOverride();
    else TextRenderer::get().setViewProjOverride(renderer.getViewProjMat());

    UILayoutCalculator::get().beginFrame();

    Vec2 rootTopLeft = renderer.getRootOrigin();
    // What a Grow or Percent root measures itself against, which is the window in screen
    // space and nothing at all in world space. Fit and Fixed roots never read it.
    Vec2 rootAvailable = renderer.getRootSize();
    for (IdType rootId : m_roots) {
        const std::vector<UILayoutNode>& solved =
            UILayoutCalculator::get().calculate(rootId, rootTopLeft, rootAvailable);

        uint maxLayer = 0;
        for (const UILayoutNode& layoutNode : solved)
            if (layoutNode.paintLayer > maxLayer) maxLayer = layoutNode.paintLayer;

        // Boxes and glyphs live in separate batches, so their relative depth is flush
        // order, not submission order -- one flush for everything would put every
        // glyph above every box no matter where the tree says they belong. Emitting a
        // layer at a time and resolving both batches per layer keeps the tree's order
        // while still costing only two draw calls per layer rather than per node.
        float scale = UIThemeManager::get().getScale();
        for (uint layer = 0; layer <= maxLayer; layer++) {
            for (const UILayoutNode& layoutNode : solved) {
                if (layoutNode.paintLayer != layer) continue;
                const UINode* node = layoutNode.node;
                if (!node) continue;
                node->draw(layoutNode.drawPos, layoutNode.size, layoutNode.clipRect);
            }
            renderer.flush();
            TextRenderer::get().flush();

            // After both flushes rather than with the boxes: a container's children share
            // its layer, so a bar drawn alongside it would end up beneath the very
            // content it scrolls -- and glyphs are a separate batch, so "above the text"
            // is a matter of flush order, not submission order. Clipped by the node's own
            // rect, not the one it hands down, since a bar is not part of what scrolls.
            bool anyBar = false;
            for (const UILayoutNode& layoutNode : solved) {
                if (layoutNode.paintLayer != layer || !layoutNode.isScrollable) continue;

                bool hovered = layoutNode.persistentKey == m_scrollHoverKey;
                for (UILayoutAxis axis : {UILayoutAxis::Vertical, UILayoutAxis::Horizontal}) {
                    UIScrollbar bar = scrollbarOf(layoutNode, axis, m_scrollbarStyle, scale);
                    drawScrollbar(
                        bar, m_scrollbarStyle, hovered && m_scrollHoverAxis == axis,
                        layoutNode.clipRect
                    );
                    anyBar = anyBar || bar.isVisible;
                }
            }
            if (anyBar) renderer.flush();
        }
    }

    TextRenderer::get().clearViewProjOverride();
}

}   // namespace Engine
