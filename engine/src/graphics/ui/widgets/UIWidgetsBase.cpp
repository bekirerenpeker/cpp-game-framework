#include "graphics/ui/widgets/UIWidgets.hpp"
#include "graphics/ui/widgets/UIWidgetsInternal.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "graphics/gl_wrappers/GlTexture.hpp"
#include "graphics/ui/UiManager.hpp"

namespace Engine {

namespace UIWidgets {

namespace {

GlTexture* loadWidgetTexture(const char* assetPath)
{
    fs::path path = FileManager::get().engineAsset(assetPath);
    if (!FileManager::get().doesPathExist(path)) {
        LOG_ERROR("widget image missing at {}", path);
        return nullptr;
    }

    return new GlTexture(path, 0, GlTexture::FilterMode::Linear, GlTexture::WrapMode::ClampToEdge);
}

}   // namespace

GlTexture* dragHandleTexture()
{
    static GlTexture* texture = loadWidgetTexture("images/resize-handle.png");
    return texture;
}

GlTexture* dropdownTexture()
{
    static GlTexture* texture = loadWidgetTexture("images/dropdown.png");
    return texture;
}

float uiScale() { return UIThemeManager::get().getScale(); }
float unscale(float value) { return value / uiScale(); }
Vec2 unscale(Vec2 value) { return value / uiScale(); }

void clear() { UIManager::get().clear(); }
UINode* getNode(IdType id) { return UIManager::get().getNode(id); }

UINodeState
openContainer(const UILayoutConfig& layout, const UIContainerStyleSpec& style, std::string_view key)
{
    return UIManager::get().openContainer(layout, style, key);
}
void closeContainer() { UIManager::get().closeContainer(); }

IdType addTextLeaf(const UILayoutConfig& layout, const UITextConfig& config)
{
    return UIManager::get().addTextLeaf(layout, config);
}

IdType addShaderLeaf(const UILayoutConfig& layout, const UIShaderConfig& config)
{
    return UIManager::get().addShaderLeaf(layout, config);
}
void removeChildren(IdType id) { UIManager::get().removeChildren(id); }

}   // namespace UIWidgets

}   // namespace Engine
