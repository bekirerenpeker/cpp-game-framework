#include "graphics/ui/UiSystem.hpp"
#include "core/Time.hpp"
#include "core/input/Input.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "core/window_management/ViewContext.hpp"
#include "core/window_management/Window.hpp"
#include "graphics/ui/UiRenderer.hpp"
#include "utils/math/MathFuncs.hpp"

namespace Engine {

static const std::vector<TextStyle> NO_SPAN_STYLES;

// Both spaces are Y-down from a top-left origin -- the single flip the UI needs lives
// in UiRenderer::uiToScreen, on the way to a Y-up GL viewport -- so a hit test is just
// the root's viewport undone.
static Vec2 screenToRootUi(Vec2 clientPos, Vec2 screenTopLeft, float pixelsPerUiUnit)
{
    if (pixelsPerUiUnit == 0.0f) return Vec2(-UNBOUNDED, -UNBOUNDED);
    return (clientPos - screenTopLeft) / pixelsPerUiUnit;
}

UiState UiSystem::begin(
    Vec2 rootSize, const LayoutConfig& rootLayout, const UiStyles& styles, std::string_view id
)
{
    beginFrameIfNeeded();

    // The window is folded in so the same widget id in two windows cannot share an
    // active/hot key, and the ordinal so two unnamed roots in one window still differ.
    UiKey seed = UiHash::combine(UiHash::FNV_OFFSET, (uint64_t)m_lastWindowId);
    m_rootKey =
        id.empty() ? UiHash::combine(seed, (uint64_t)m_rootOrdinal) : UiHash::combine(seed, id);
    m_rootOrdinal++;

    // Read before the counters reset: the root is an element like any other, so it can
    // report a drag on its own edge -- which is the only way to resize a root, its size
    // being an argument rather than something the solver derives.
    UiState state = makeState(m_rootKey);

    m_rootSize = rootSize;
    m_elementCount = 0;
    m_textLeafCount = 0;
    m_imageLeafCount = 0;
    m_openStack.clear();
    m_isBuilding = true;

    // The root is Fixed to the size handed in: shrinking is the only thing that makes
    // text wrap, and shrinking only happens when a real constraint comes from above.
    LayoutConfig config = rootLayout;
    config.width = SizeSpec::fixed(rootSize.x);
    config.height = SizeSpec::fixed(rootSize.y);

    state.index =
        pushElement(UiElementType::Container, config, resolveStyle(styles, state), m_rootKey);
    m_openStack.push_back({state.index, m_rootKey, 0});
    return state;
}

void UiSystem::draw()
{
    if (!m_isBuilding) {
        LOG_WARNING("UiSystem::draw() called without a matching begin(); nothing to lay out");
        return;
    }

    if (m_openStack.size() > 1 && !m_warnedUnbalanced) {
        LOG_WARNING(
            "UiSystem: {} container(s) still open at draw(); closing them automatically",
            m_openStack.size() - 1
        );
        m_warnedUnbalanced = true;
    }
    m_openStack.clear();
    m_isBuilding = false;

    m_layout.reset(m_elementCount);
    for (uint i = 0; i < m_elementCount; i++) {
        const UiElement& element = m_elements[i];
        LayoutInput input;
        input.config = element.layout;
        input.measurer = element.measurer;
        input.parent = element.parent;
        input.sourceIndex = i;
        m_layout.addNode(input);
    }

    const std::vector<LayoutNode>& nodes = m_layout.compute(m_rootSize);

    // Appended, never cleared here: a frame can hold several begin/draw cycles and they
    // all have to survive into the next frame's hit test. Only the frame boundary clears.
    RootViewport root;
    root.windowId = m_lastWindowId;
    root.screenTopLeft = UiRenderer::get().getScreenTopLeft();
    root.pixelsPerUiUnit = UiRenderer::get().getPixelsPerUiUnit();

    uint rootIndex = (uint)m_currRoots.size();
    uint entryBase = (uint)m_currRects.size();
    m_currRoots.push_back(root);

    for (uint i = 0; i < m_elementCount && i < nodes.size(); i++) {
        CachedRect rect;
        rect.key = m_elementInteractions[i].key;
        rect.hitTestable = m_elementInteractions[i].hitTestable;
        rect.parentEntry =
            m_elements[i].parent == NO_NODE ? NO_NODE : entryBase + m_elements[i].parent;
        rect.pos = nodes[i].pos;
        rect.size = nodes[i].size;
        rect.rootIndex = rootIndex;
        m_currRects.push_back(rect);
    }

    UiRenderer::get().render(m_elements, m_elementCount, nodes);
}

void UiSystem::shutdown()
{
    for (TextLeaf* leaf : m_textLeaves) delete leaf;
    for (ImageLeaf* leaf : m_imageLeaves) delete leaf;
    m_textLeaves.clear();
    m_imageLeaves.clear();
    m_textLeafCount = 0;
    m_imageLeafCount = 0;
}

void UiSystem::beginFrameIfNeeded()
{
    uint64_t frame = Time::get().getFrameCount();
    IdType windowId = ViewContext::get().getActiveWindowId();

    if (frame != m_lastFrame) {
        m_lastFrame = frame;
        // Forces the per-window sample below to run again for whichever window opens
        // the new frame, even if it is the same one that closed the last.
        m_lastWindowId = INVALID_ID;

        m_prevRects.swap(m_currRects);
        m_prevRoots.swap(m_currRoots);
        m_currRects.clear();
        m_currRoots.clear();

        m_prevRectByKey.clear();
        for (uint i = 0; i < m_prevRects.size(); i++) m_prevRectByKey[m_prevRects[i].key] = i;
    }

    if (windowId == m_lastWindowId) return;

    // Mouse position and button edges are both per-window state on Input, so everything
    // downstream of them is resolved once per window rather than once per frame.
    m_lastWindowId = windowId;
    m_rootOrdinal = 0;
    sampleMouse();
    resolveHover();
    updateActive();
}

void UiSystem::sampleMouse()
{
    Window* window = ViewContext::get().getActiveWindow();
    m_mouseValid = window != nullptr;
    if (!m_mouseValid) {
        m_mouseScreenDelta = VEC2_ZERO;
        if (!m_warnedNoWindow) {
            LOG_WARNING("UiSystem: no active window; UI interaction is disabled");
            m_warnedNoWindow = true;
        }
        return;
    }

    // Input reports window-centred pixels with +Y up; the UI works in client pixels
    // from the top-left with +Y down, which is the same orientation as layout space.
    Vec2 raw = Input::get().getMousePos();
    Vec2 clientPos(raw.x + window->getWidth() * 0.5f, window->getHeight() * 0.5f - raw.y);

    auto it = m_lastMouseScreenPos.find(m_lastWindowId);
    m_mouseScreenDelta = it == m_lastMouseScreenPos.end() ? VEC2_ZERO : clientPos - it->second;
    m_lastMouseScreenPos[m_lastWindowId] = clientPos;
    m_mouseScreenPos = clientPos;
}

void UiSystem::resolveHover()
{
    m_hotKey = NO_UI_KEY;
    m_hoverChain.clear();

    m_rootMouseUi.resize(m_prevRoots.size());
    for (uint i = 0; i < m_prevRoots.size(); i++)
        m_rootMouseUi[i] = screenToRootUi(
            m_mouseScreenPos, m_prevRoots[i].screenTopLeft, m_prevRoots[i].pixelsPerUiUnit
        );

    if (!m_mouseValid) return;

    // Backwards, because rects were cached in preorder and roots in draw order, so the
    // last rect containing the point is the one painted on top. This mirrors
    // UiRenderer's own iteration -- a z-index or a floating-last pass there would have
    // to be mirrored here too.
    for (uint i = (uint)m_prevRects.size(); i-- > 0;) {
        const CachedRect& rect = m_prevRects[i];
        if (!rect.hitTestable) continue;
        if (m_prevRoots[rect.rootIndex].windowId != m_lastWindowId) continue;

        Vec2 mouse = m_rootMouseUi[rect.rootIndex];
        if (mouse.x < rect.pos.x || mouse.x > rect.pos.x + rect.size.x) continue;
        if (mouse.y < rect.pos.y || mouse.y > rect.pos.y + rect.size.y) continue;

        m_hotKey = rect.key;
        for (uint entry = i; entry != NO_NODE; entry = m_prevRects[entry].parentEntry)
            m_hoverChain.push_back(m_prevRects[entry].key);
        return;
    }
}

void UiSystem::updateActive()
{
    Input& input = Input::get();
    m_pressedKey = NO_UI_KEY;
    m_releasedKey = NO_UI_KEY;
    m_clickedKey = NO_UI_KEY;

    if (m_activeKey == NO_UI_KEY) {
        if (m_mouseValid && m_hotKey != NO_UI_KEY && input.mouseButtonPressed(MouseButton::Left)) {
            m_activeKey = m_hotKey;
            m_activeWindowId = m_lastWindowId;
            m_pressedKey = m_hotKey;
            m_pressScreenPos = m_mouseScreenPos;
            m_isDragging = false;
        }
        return;
    }

    // Only the window the press started in may advance or end it, or iterating the other
    // windows would read their (independent) button edges against this press.
    if (m_activeWindowId != m_lastWindowId) return;

    // Held rather than the release edge alone: Input polls, so a press and release inside
    // one frame produces no edge and would otherwise leave the element active forever.
    if (input.mouseButtonHeld(MouseButton::Left) && !input.mouseButtonReleased(MouseButton::Left)) {
        if (!m_isDragging &&
            (m_mouseScreenPos - m_pressScreenPos).magnitude() > DRAG_THRESHOLD_PIXELS)
            m_isDragging = true;
        return;
    }

    m_releasedKey = m_activeKey;
    if (m_hotKey == m_activeKey) m_clickedKey = m_activeKey;

    // Cleared even when the active element was not rebuilt this frame, or a widget that
    // stopped being emitted mid-press would stay active for the rest of the process.
    m_activeKey = NO_UI_KEY;
    m_activeWindowId = INVALID_ID;
    m_isDragging = false;
}

UiKey UiSystem::nextKey(std::string_view id) const
{
    if (m_openStack.empty()) return m_rootKey;

    const OpenEntry& parent = m_openStack.back();
    if (!id.empty()) return UiHash::combine(parent.key, id);
    return UiHash::combine(parent.key, (uint64_t)parent.childCounter);
}

UiState UiSystem::makeState(UiKey key) const
{
    UiState state;
    state.key = key;

    auto it = m_prevRectByKey.find(key);
    if (it == m_prevRectByKey.end()) return state;

    const CachedRect& rect = m_prevRects[it->second];
    state.hasRect = true;
    state.pos = rect.pos;
    state.size = rect.size;

    // While something is held, only it may report hover, so dragging a slider does not
    // light up whatever the cursor passes over. Suppressed here rather than in
    // resolveHover, because isClicked still has to compare the raw hot key.
    bool otherHeld = m_activeKey != NO_UI_KEY && m_activeKey != key;
    state.isHoveredDirect = !otherHeld && m_hotKey == key;
    state.isHovered = !otherHeld && isInHoverChain(key);

    state.isPressed = m_pressedKey == key;
    state.isHeld = m_activeKey == key;
    state.isReleased = m_releasedKey == key;
    state.isClicked = m_clickedKey == key;
    state.isDragging = m_isDragging && state.isHeld;

    if (m_mouseValid) {
        state.mouseLocal = m_rootMouseUi[rect.rootIndex] - rect.pos;
        state.mouseNormalized = Vec2(
            rect.size.x > 0.0f ? state.mouseLocal.x / rect.size.x : 0.0f,
            rect.size.y > 0.0f ? state.mouseLocal.y / rect.size.y : 0.0f
        );
        state.mouseDelta = screenToUiDelta(m_mouseScreenDelta, rect.rootIndex);
        if (state.isHeld || state.isReleased)
            state.dragDelta = screenToUiDelta(m_mouseScreenPos - m_pressScreenPos, rect.rootIndex);
    }
    return state;
}

bool UiSystem::isInHoverChain(UiKey key) const
{
    for (UiKey chained : m_hoverChain)
        if (chained == key) return true;
    return false;
}

Vec2 UiSystem::screenToUiDelta(Vec2 screenDelta, uint rootIndex) const
{
    float pixelsPerUiUnit = m_prevRoots[rootIndex].pixelsPerUiUnit;
    if (pixelsPerUiUnit == 0.0f) return VEC2_ZERO;

    // A pure scale, not a difference of two mapped points: the viewport can move
    // between frames and that must not read as the cursor having moved.
    return screenDelta / pixelsPerUiUnit;
}

UiStyle UiSystem::resolveStyle(const UiStyles& styles, const UiState& state)
{
    UiStyle resolved = styles.normal;
    if (state.isHovered) styles.hovered.applyTo(resolved);
    if (state.isHeld) styles.pressed.applyTo(resolved);
    return resolved;
}

UiState UiSystem::openContainer(const LayoutConfig& layout, const UiStyles& styles)
{
    return openContainer({}, layout, styles);
}

UiState
UiSystem::openContainer(std::string_view id, const LayoutConfig& layout, const UiStyles& styles)
{
    UiKey key = nextKey(id);
    UiState state = makeState(key);

    state.index = pushElement(UiElementType::Container, layout, resolveStyle(styles, state), key);
    m_openStack.push_back({state.index, key, 0});
    return state;
}

void UiSystem::closeContainer()
{
    // The root always stays open until draw(), so an extra close is a no-op rather
    // than something that would reparent the rest of the frame onto nothing.
    if (m_openStack.size() <= 1) return;
    m_openStack.pop_back();
}

void UiSystem::setHitTestable(uint index, bool value)
{
    if (index < m_elementCount) m_elementInteractions[index].hitTestable = value;
}

UiState UiSystem::addDivider(float thickness, const UiStyles& styles, std::string_view id)
{
    // Grow, not Percent(100): a percent width is measured against the parent's content
    // box before the parent has one when that parent is Fit, so a rule in a Fit column
    // would collapse. Grow seeds at max-content -- zero here -- and takes what is left.
    LayoutConfig config;
    config.width = SizeSpec::grow();
    config.height = SizeSpec::fixed(Math::max(0.0f, thickness));

    UiState state = openContainer(id, config, styles);
    closeContainer();
    return state;
}

UiState UiSystem::addText(std::string_view text, const LayoutConfig& layout)
{
    return addText(text, m_defaultTextStyle, NO_SPAN_STYLES, layout);
}

UiState
UiSystem::addText(std::string_view text, const TextStyle& textStyle, const LayoutConfig& layout)
{
    return addText(text, textStyle, NO_SPAN_STYLES, layout);
}

UiState UiSystem::addText(
    std::string_view text, const TextStyle& textStyle, const std::vector<TextStyle>& spanStyles,
    const LayoutConfig& layout, bool wrap, bool fixedLineHeight
)
{
    UiKey key = nextKey({});
    UiState state = makeState(key);

    state.index = pushElement(UiElementType::Text, layout, UiStyle {}, key);
    UiElement& element = m_elements[state.index];
    element.text.assign(text);
    element.textStyle = textStyle;
    element.font = m_defaultFont;

    // A label must not steal the hit from the button wrapping it, and text is the one
    // element type that is never interactive on its own.
    m_elementInteractions[state.index].hitTestable = false;

    if (!m_defaultFont && !m_warnedNoFont) {
        LOG_WARNING("UiSystem: text added before setDefaultFont(); it will measure as empty");
        m_warnedNoFont = true;
    }

    TextLeaf* leaf = acquireTextLeaf();
    leaf->set(m_defaultFont, element.text, textStyle, spanStyles, wrap, fixedLineHeight);
    element.measurer = leaf;

    if (!leaf->areTagsClosed() && !m_warnedUnclosedTag) {
        LOG_WARNING("UiSystem: unclosed /s tag in \"{}\"; it styles to end of string", text);
        m_warnedUnclosedTag = true;
    }
    return state;
}

UiState UiSystem::addButton(
    std::string_view label, const LayoutConfig& layout, const UiStyles& styles, std::string_view id
)
{
    LayoutConfig config = layout;
    if (config.padding.horizontal() == 0.0f && config.padding.vertical() == 0.0f)
        config.padding = m_defaultButtonPadding;

    // The label doubles as the id, so a button is stable across frames with no ceremony.
    // Two buttons sharing a label under one parent therefore share state -- pass an
    // explicit id for those.
    UiState state = openContainer(id.empty() ? label : id, config, styles);
    m_elements[state.index].type = UiElementType::Button;
    m_elements[state.index].text.assign(label);

    // UiRenderer reads a text element's style off its TextLeaf, so a per-state colour
    // has to be baked in before the leaf is built rather than assigned afterwards.
    TextStyle labelStyle = m_defaultTextStyle;
    labelStyle.color = m_elements[state.index].style.contentColor;
    addText(label, labelStyle);
    closeContainer();
    return state;
}

UiState UiSystem::addImage(
    const GlTexture* texture, Vec2 nativeSize, float aspectRatio, const LayoutConfig& layout,
    const UiStyles& styles, std::string_view id
)
{
    UiKey key = nextKey(id);
    UiState state = makeState(key);

    state.index = pushElement(UiElementType::Image, layout, resolveStyle(styles, state), key);
    UiElement& element = m_elements[state.index];
    element.texture = texture;

    ImageLeaf* leaf = acquireImageLeaf();
    leaf->set(texture, nativeSize, aspectRatio);
    element.measurer = leaf;
    return state;
}

uint UiSystem::pushElement(
    UiElementType type, const LayoutConfig& layout, const UiStyle& style, UiKey key
)
{
    uint index = m_elementCount++;
    // Grown but never shrunk, so each element's string keeps its heap buffer across
    // frames instead of being freed and regrown every rebuild.
    if (m_elements.size() < m_elementCount) m_elements.resize(m_elementCount);
    if (m_elementInteractions.size() < m_elementCount) m_elementInteractions.resize(m_elementCount);

    UiElement& element = m_elements[index];
    element.layout = layout;
    element.style = style;
    element.textStyle = m_defaultTextStyle;
    element.text.clear();
    element.font = nullptr;
    element.texture = nullptr;
    element.measurer = nullptr;
    element.type = type;
    element.parent = m_openStack.empty() ? NO_NODE : m_openStack.back().elementIndex;

    m_elementInteractions[index] = {key, true};
    // Consumed here rather than in nextKey so that computing a key is side-effect free
    // and a builder can read its state before deciding to push at all.
    if (!m_openStack.empty()) m_openStack.back().childCounter++;
    return index;
}

TextLeaf* UiSystem::acquireTextLeaf()
{
    if (m_textLeafCount == m_textLeaves.size()) m_textLeaves.push_back(new TextLeaf());
    return m_textLeaves[m_textLeafCount++];
}

ImageLeaf* UiSystem::acquireImageLeaf()
{
    if (m_imageLeafCount == m_imageLeaves.size()) m_imageLeaves.push_back(new ImageLeaf());
    return m_imageLeaves[m_imageLeafCount++];
}

}   // namespace Engine
