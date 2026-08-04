#include "graphics/ui/UIWidgets.hpp"
#include "graphics/ui/UiManager.hpp"

namespace Engine {

namespace UIWidgets {

// base components interfaces
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

}   // namespace UIWidgets

}   // namespace Engine
