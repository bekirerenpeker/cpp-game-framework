#include "graphics/ui/UiSystem.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "graphics/Renderer.hpp"

namespace Engine {

void UiSystem::setDebugViewport(Vec2 worldTopLeft, float worldUnitsPerUiUnit)
{
    m_debugDrawer.setViewport(worldTopLeft, worldUnitsPerUiUnit);
}

void UiSystem::begin(Vec2 rootSize, const LayoutConfig& rootLayout)
{
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

    uint root = pushElement(UiElementType::Container, config, UiStyle {});
    m_openStack.push_back(root);
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

    m_layout.compute(m_rootSize);
    m_debugDrawer.draw(m_layout.getNodes());
    // The debug boxes go through the sprite batch, so they have to be flushed before
    // any text the scene submits afterwards.
    Renderer::get().endScene();
}

void UiSystem::shutdown()
{
    for (TextLeaf* leaf : m_textLeaves) delete leaf;
    for (ImageLeaf* leaf : m_imageLeaves) delete leaf;
    m_textLeaves.clear();
    m_imageLeaves.clear();
    m_textLeafCount = 0;
    m_imageLeafCount = 0;

    m_debugDrawer.release();
}

uint UiSystem::openContainer(const LayoutConfig& layout, const UiStyle& style)
{
    uint index = pushElement(UiElementType::Container, layout, style);
    m_openStack.push_back(index);
    return index;
}

void UiSystem::closeContainer()
{
    // The root always stays open until draw(), so an extra close is a no-op rather
    // than something that would reparent the rest of the frame onto nothing.
    if (m_openStack.size() <= 1) return;
    m_openStack.pop_back();
}

uint UiSystem::addText(std::string_view text, const LayoutConfig& layout, bool wrap)
{
    return addText(text, m_defaultTextStyle, layout, wrap);
}

uint UiSystem::addText(
    std::string_view text, const TextStyle& textStyle, const LayoutConfig& layout, bool wrap
)
{
    uint index = pushElement(UiElementType::Text, layout, UiStyle {});
    UiElement& element = m_elements[index];
    element.text.assign(text);
    element.textStyle = textStyle;
    element.font = m_defaultFont;

    if (!m_defaultFont && !m_warnedNoFont) {
        LOG_WARNING("UiSystem: text added before setDefaultFont(); it will measure as empty");
        m_warnedNoFont = true;
    }

    TextLeaf* leaf = acquireTextLeaf();
    leaf->set(m_defaultFont, element.text, textStyle, wrap);
    element.measurer = leaf;
    return index;
}

uint UiSystem::addButton(std::string_view label, const LayoutConfig& layout, const UiStyle& style)
{
    LayoutConfig config = layout;
    if (config.padding.horizontal() == 0.0f && config.padding.vertical() == 0.0f)
        config.padding = m_defaultButtonPadding;

    uint index = openContainer(config, style);
    m_elements[index].type = UiElementType::Button;
    m_elements[index].text.assign(label);
    addText(label);
    closeContainer();
    return index;
}

uint UiSystem::addImage(
    const GlTexture* texture, Vec2 nativeSize, float aspectRatio, const LayoutConfig& layout,
    const UiStyle& style
)
{
    uint index = pushElement(UiElementType::Image, layout, style);
    UiElement& element = m_elements[index];
    element.texture = texture;

    ImageLeaf* leaf = acquireImageLeaf();
    leaf->set(texture, nativeSize, aspectRatio);
    element.measurer = leaf;
    return index;
}

uint UiSystem::pushElement(UiElementType type, const LayoutConfig& layout, const UiStyle& style)
{
    uint index = m_elementCount++;
    // Grown but never shrunk, so each element's string keeps its heap buffer across
    // frames instead of being freed and regrown every rebuild.
    if (m_elements.size() < m_elementCount) m_elements.resize(m_elementCount);

    UiElement& element = m_elements[index];
    element.layout = layout;
    element.style = style;
    element.textStyle = m_defaultTextStyle;
    element.text.clear();
    element.font = nullptr;
    element.texture = nullptr;
    element.measurer = nullptr;
    element.type = type;
    element.parent = m_openStack.empty() ? NO_NODE : m_openStack.back();
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
