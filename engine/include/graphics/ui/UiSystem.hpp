#pragma once

#include "graphics/ui/layout/LayoutCalculator.hpp"
#include "graphics/ui/elements/UiElement.hpp"
#include "graphics/ui/elements/UiLeaves.hpp"
#include "graphics/ui/UiInteraction.hpp"
#include "utils/Singleton.hpp"
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Engine {

class UiSystem : public Singleton<UiSystem>
{
    friend class Singleton<UiSystem>;

  private:
    struct OpenEntry
    {
        uint elementIndex = NO_NODE;
        UiKey key = NO_UI_KEY;
        uint childCounter = 0;
    };

    struct ElementInteraction
    {
        UiKey key = NO_UI_KEY;
        bool hitTestable = true;
    };

    // One begin/draw cycle's viewport, shared by every rect that cycle produced.
    struct RootViewport
    {
        IdType windowId = INVALID_ID;
        Vec2 screenTopLeft = VEC2_ZERO;
        float pixelsPerUiUnit = 1.0f;
    };

    struct CachedRect
    {
        UiKey key = NO_UI_KEY;
        uint parentEntry = NO_NODE;
        Vec2 pos = VEC2_ZERO;
        Vec2 size = VEC2_ZERO;
        uint rootIndex = 0;
        bool hitTestable = true;
    };

    static constexpr float DRAG_THRESHOLD_PIXELS = 3.0f;

    std::vector<UiElement> m_elements;
    std::vector<ElementInteraction> m_elementInteractions;
    std::vector<OpenEntry> m_openStack;
    std::vector<TextLeaf*> m_textLeaves;
    std::vector<ImageLeaf*> m_imageLeaves;

    std::vector<CachedRect> m_prevRects;
    std::vector<CachedRect> m_currRects;
    std::vector<RootViewport> m_prevRoots;
    std::vector<RootViewport> m_currRoots;
    std::vector<Vec2> m_rootMouseUi;
    std::vector<UiKey> m_hoverChain;
    std::unordered_map<UiKey, uint> m_prevRectByKey;
    std::unordered_map<IdType, Vec2> m_lastMouseScreenPos;

    LayoutCalculator m_layout;

    const Font* m_defaultFont = nullptr;
    TextStyle m_defaultTextStyle;
    LayoutEdges m_defaultButtonPadding {10.0f, 10.0f, 6.0f, 6.0f};

    Vec2 m_rootSize = VEC2_ZERO;
    Vec2 m_mouseScreenPos = VEC2_ZERO;
    Vec2 m_mouseScreenDelta = VEC2_ZERO;
    Vec2 m_pressScreenPos = VEC2_ZERO;

    UiKey m_rootKey = NO_UI_KEY;
    UiKey m_hotKey = NO_UI_KEY;
    UiKey m_activeKey = NO_UI_KEY;
    UiKey m_pressedKey = NO_UI_KEY;
    UiKey m_releasedKey = NO_UI_KEY;
    UiKey m_clickedKey = NO_UI_KEY;
    IdType m_activeWindowId = INVALID_ID;
    IdType m_lastWindowId = INVALID_ID;

    uint64_t m_lastFrame = (uint64_t)-1;
    uint m_rootOrdinal = 0;
    uint m_elementCount = 0;
    uint m_textLeafCount = 0;
    uint m_imageLeafCount = 0;
    bool m_isBuilding = false;
    bool m_isDragging = false;
    bool m_mouseValid = false;
    bool m_warnedUnbalanced = false;
    bool m_warnedNoFont = false;
    bool m_warnedUnclosedTag = false;
    bool m_warnedNoWindow = false;

  public:
    void setDefaultFont(const Font* font) { m_defaultFont = font; }
    void setDefaultTextStyle(const TextStyle& style) { m_defaultTextStyle = style; }
    void setDefaultButtonPadding(const LayoutEdges& padding) { m_defaultButtonPadding = padding; }
    const Font* getDefaultFont() const { return m_defaultFont; }

    UiState begin(
        Vec2 rootSize, const LayoutConfig& rootLayout = {}, const UiStyles& styles = {},
        std::string_view id = {}
    );
    void draw();
    void shutdown();

    UiState openContainer(const LayoutConfig& layout = {}, const UiStyles& styles = {});
    UiState openContainer(
        std::string_view id, const LayoutConfig& layout = {}, const UiStyles& styles = {}
    );
    void closeContainer();
    void setHitTestable(uint index, bool value);

    UiState addText(std::string_view text, const LayoutConfig& layout = {});
    UiState
    addText(std::string_view text, const TextStyle& textStyle, const LayoutConfig& layout = {});
    UiState addText(
        std::string_view text, const TextStyle& textStyle, const std::vector<TextStyle>& spanStyles,
        const LayoutConfig& layout = {}, bool wrap = true, bool fixedLineHeight = false
    );
    UiState addButton(
        std::string_view label, const LayoutConfig& layout = {}, const UiStyles& styles = {},
        std::string_view id = {}
    );
    UiState addImage(
        const GlTexture* texture, Vec2 nativeSize, float aspectRatio = 0.0f,
        const LayoutConfig& layout = {}, const UiStyles& styles = {}, std::string_view id = {}
    );

    const std::vector<UiElement>& getElements() const { return m_elements; }
    const std::vector<LayoutNode>& getLayoutNodes() const { return m_layout.getNodes(); }
    const std::vector<LayoutLine>& getLayoutLines() const { return m_layout.getLines(); }
    uint getElementCount() const { return m_elementCount; }
    UiKey getHotKey() const { return m_hotKey; }
    UiKey getActiveKey() const { return m_activeKey; }

  private:
    UiSystem() = default;
    ~UiSystem() = default;

    void beginFrameIfNeeded();
    void sampleMouse();
    void resolveHover();
    void updateActive();

    UiKey nextKey(std::string_view id) const;
    UiState makeState(UiKey key) const;
    bool isInHoverChain(UiKey key) const;
    Vec2 screenToUiDelta(Vec2 screenDelta, uint rootIndex) const;
    static UiStyle resolveStyle(const UiStyles& styles, const UiState& state);

    uint
    pushElement(UiElementType type, const LayoutConfig& layout, const UiStyle& style, UiKey key);
    TextLeaf* acquireTextLeaf();
    ImageLeaf* acquireImageLeaf();
};

}   // namespace Engine
