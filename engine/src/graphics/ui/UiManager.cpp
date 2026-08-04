#include "graphics/ui/UiManager.hpp"
#include "core/input/Input.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "graphics/text/TextRenderer.hpp"
#include "graphics/ui/UILayoutCalculator.hpp"
#include "graphics/ui/UIRenderer.hpp"
#include "utils/math/MathFuncs.hpp"
#include <functional>

namespace Engine {

namespace {

uint64_t hashCombine(uint64_t seed, uint64_t value)
{
    return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
}

// Distinct seeds so an unnamed node's positional index and a named node's hash never
// collide into the same key.
constexpr uint64_t POSITIONAL_KEY_SEED = 1;
constexpr uint64_t NAMED_KEY_SEED = 2;

uint64_t localKeyOf(std::string_view key, uint64_t positionalIndex)
{
    return key.empty() ? hashCombine(POSITIONAL_KEY_SEED, positionalIndex) :
                         hashCombine(NAMED_KEY_SEED, std::hash<std::string_view> {}(key));
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

bool containsMouse(const UILayoutNode& node, Vec2 mouse)
{
    Vec2 half = node.size * 0.5f;
    Vec2 delta = mouse - node.drawPos;
    if (Math::abs(delta.x) > half.x || Math::abs(delta.y) > half.y) return false;

    const Vec4& clip = node.clipRect;
    return mouse.x >= clip.x && mouse.y >= clip.y && mouse.x <= clip.z && mouse.y <= clip.w;
}

UINodeState
computeState(IdType id, const UILayoutNode* prev, bool hovered, bool hoveredDirectly, bool active)
{
    if (!prev) return {id};

    Vec2 half = prev->size * 0.5f;
    // Both sides come from the same space -- the mouse through UIRenderer, drawPos
    // baked by the solver -- so screen and world hit test through one path. drawPos is
    // centre-anchored and Y-up; flip into the box's own top-left/Y-down space so
    // local/relative match the rest of the UI instead of the render convention.
    Vec2 centerDelta = UIRenderer::get().getMouseUiPos() - prev->drawPos;
    Vec2 local = Vec2(centerDelta.x + half.x, half.y - centerDelta.y);
    Vec2 relative = prev->size.x > 0.0f && prev->size.y > 0.0f ? local / prev->size : VEC2_ZERO;

    // The button states ride on the bubbled hover, so a click inside a child still
    // reaches the containers around it. Two *overlapping* containers still cannot both
    // react, since only the topmost one's ancestors are in the chain at all.
    return {
        id,
        hovered,
        hoveredDirectly,
        active,
        hovered && Input::get().mouseButtonPressed(MouseButton::Left),
        hovered && Input::get().mouseButtonReleased(MouseButton::Left),
        hovered && Input::get().mouseButtonHeld(MouseButton::Left),
        relative,
        local,
        prev->pos,
        prev->size
    };
}

// Lowest priority first, so a pressed button keeps the rest of its hover look instead
// of falling back to the base. onHeld reads isActive rather than isHeld so a grip
// dragged off itself stays lit until release, since isHeld dies the moment hover does.
UIContainerStyle resolveStyle(const UIContainerStyleSpec& spec, const UINodeState& state)
{
    UIContainerStyle style = spec.base();
    if (state.isHovered) style.combine(spec.onHover);
    if (state.isActive) style.combine(spec.onHeld);
    if (state.isPressed) style.combine(spec.onPressed);
    if (state.isReleased) style.combine(spec.onReleased);

    // Filled once the states have all merged, so every field is engaged by the time the
    // node is stored -- the renderer then reads plain values instead of re-deciding a
    // default per field, and the defaults themselves live only in UIContainerStyle.
    style.fillDefaults();
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

    Vec2 mouse = UIRenderer::get().getMouseUiPos();
    const std::vector<UILayoutNode>& prev = UILayoutCalculator::get().getPrevFrameNodes();
    Input& input = Input::get();

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
        if (hit != NO_LAYOUT_NODE && input.mouseButtonPressed(MouseButton::Left))
            m_activeKey = prev[hit].persistentKey;
    }

    if (hit == NO_LAYOUT_NODE) return;

    // The hit node first, then every ancestor. Walking up is what keeps a panel lit
    // while the cursor is on one of its own children; stopping at the hit node alone
    // reads as the panel flickering off whenever the cursor crosses its contents.
    for (uint i = hit; i != NO_LAYOUT_NODE; i = prev[i].parent)
        m_hoveredKeys.push_back(prev[i].persistentKey);
}

bool UIManager::isKeyHovered(uint64_t key) const
{
    for (uint64_t hovered : m_hoveredKeys)
        if (hovered == key) return true;
    return false;
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

    if (m_openStack.empty()) {
        node->parent = INVALID_ID;
        node->persistentKey = hashCombine(0, localKeyOf(key, (uint64_t)m_roots.size()));
        m_roots.push_back(id);
        return {id, false, false, false, false};
    }

    IdType parent = m_openStack.back();
    node->parent = parent;

    UINode* parentNode = m_nodes.get(parent);
    if (!parentNode) return {id, false, false, false, false};

    node->persistentKey =
        hashCombine(parentNode->persistentKey, localKeyOf(key, (uint64_t)parentNode->childCount));

    if (parentNode->lastChild == INVALID_ID) parentNode->firstChild = id;
    else m_nodes.get(parentNode->lastChild)->nextSibling = id;

    parentNode->lastChild = id;
    parentNode->childCount++;

    return {id, false, false, false, false};
}

UINodeState UIManager::openContainer(
    const UILayoutConfig& layout, const UIContainerStyleSpec& style, std::string_view key
)
{
    UINodeState state = addNode(layout, key);
    UINode* node = m_nodes.get(state.id);
    if (node) {
        node->prevFrameLayout = UILayoutCalculator::get().getPrevFrameLayout(node->persistentKey);
        bool hovered = isKeyHovered(node->persistentKey);
        bool direct = !m_hoveredKeys.empty() && m_hoveredKeys.front() == node->persistentKey;
        bool active = m_activeKey != NO_KEY && node->persistentKey == m_activeKey;
        state = computeState(state.id, node->prevFrameLayout, hovered, direct, active);

        // Flattened here rather than stored, so the node still carries one final style
        // and nothing downstream has to know a state ever existed. The caller can still
        // rewrite node->style afterwards for anything a constant cannot express.
        node->style = resolveStyle(style, state);
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
    UINodeState state = addNode(layout);
    UINode* node = m_nodes.get(state.id);
    if (!node) return state.id;

    UITextLeafData* leaf = new UITextLeafData(config);
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
    for (IdType rootId : m_roots) {
        const std::vector<UILayoutNode>& solved =
            UILayoutCalculator::get().calculate(rootId, rootTopLeft);

        uint maxLayer = 0;
        for (const UILayoutNode& layoutNode : solved)
            if (layoutNode.paintLayer > maxLayer) maxLayer = layoutNode.paintLayer;

        // Boxes and glyphs live in separate batches, so their relative depth is flush
        // order, not submission order -- one flush for everything would put every
        // glyph above every box no matter where the tree says they belong. Emitting a
        // layer at a time and resolving both batches per layer keeps the tree's order
        // while still costing only two draw calls per layer rather than per node.
        for (uint layer = 0; layer <= maxLayer; layer++) {
            for (const UILayoutNode& layoutNode : solved) {
                if (layoutNode.paintLayer != layer) continue;
                const UINode* node = layoutNode.node;
                if (!node) continue;
                node->draw(layoutNode.drawPos, layoutNode.size, layoutNode.clipRect);
            }
            renderer.flush();
            TextRenderer::get().flush();
        }
    }

    TextRenderer::get().clearViewProjOverride();
}

}   // namespace Engine
