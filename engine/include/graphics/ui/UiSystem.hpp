#pragma once

#include "graphics/ui/LayoutComputer.hpp"
#include "graphics/ui/UiElement.hpp"
#include "graphics/ui/UiLeaves.hpp"
#include "utils/Singleton.hpp"
#include <string_view>
#include <vector>

namespace Engine {

class UiSystem : public Singleton<UiSystem>
{
    friend class Singleton<UiSystem>;

  private:
    std::vector<UiElement> m_elements;
    std::vector<uint> m_openStack;
    std::vector<TextLeaf*> m_textLeaves;
    std::vector<ImageLeaf*> m_imageLeaves;

    LayoutComputer m_layout;

    const Font* m_defaultFont = nullptr;
    TextStyle m_defaultTextStyle;
    LayoutEdges m_defaultButtonPadding {10.0f, 10.0f, 6.0f, 6.0f};

    Vec2 m_rootSize = VEC2_ZERO;
    uint m_elementCount = 0;
    uint m_textLeafCount = 0;
    uint m_imageLeafCount = 0;
    bool m_isBuilding = false;
    bool m_warnedUnbalanced = false;
    bool m_warnedNoFont = false;
    bool m_warnedUnclosedTag = false;

  public:
    void setDefaultFont(const Font* font) { m_defaultFont = font; }
    void setDefaultTextStyle(const TextStyle& style) { m_defaultTextStyle = style; }
    void setDefaultButtonPadding(const LayoutEdges& padding) { m_defaultButtonPadding = padding; }
    const Font* getDefaultFont() const { return m_defaultFont; }

    void begin(Vec2 rootSize, const LayoutConfig& rootLayout = {});
    void draw();
    void shutdown();

    uint openContainer(const LayoutConfig& layout = {}, const UiStyle& style = {});
    void closeContainer();

    uint addText(std::string_view text, const LayoutConfig& layout = {});
    uint
    addText(std::string_view text, const TextStyle& textStyle, const LayoutConfig& layout = {});
    uint addText(
        std::string_view text, const TextStyle& textStyle, const std::vector<TextStyle>& spanStyles,
        const LayoutConfig& layout = {}, bool wrap = true, bool fixedLineHeight = false
    );
    uint
    addButton(std::string_view label, const LayoutConfig& layout = {}, const UiStyle& style = {});
    uint addImage(
        const GlTexture* texture, Vec2 nativeSize, float aspectRatio = 0.0f,
        const LayoutConfig& layout = {}, const UiStyle& style = {}
    );

    const std::vector<UiElement>& getElements() const { return m_elements; }
    const std::vector<LayoutNode>& getLayoutNodes() const { return m_layout.getNodes(); }
    const std::vector<LayoutLine>& getLayoutLines() const { return m_layout.getLines(); }
    uint getElementCount() const { return m_elementCount; }

  private:
    UiSystem() = default;
    ~UiSystem() = default;

    uint pushElement(UiElementType type, const LayoutConfig& layout, const UiStyle& style);
    TextLeaf* acquireTextLeaf();
    ImageLeaf* acquireImageLeaf();
};

}   // namespace Engine
