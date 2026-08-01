#pragma once

#include "graphics/ui/UINode.hpp"
#include "graphics/ui/leaf_types/UITextLeafData.hpp"
#include "utils/IdIndexedVector.hpp"
#include "utils/Singleton.hpp"
#include <string_view>
#include <vector>

namespace Engine {

struct UINodeState
{
    IdType id;
    bool isHovered, isPressed, isReleased, isHeld;
};

class UIManager : public Singleton<UIManager>
{
    friend class Singleton<UIManager>;

  private:
    IdIndexedVector<UINode> m_nodes;
    std::vector<IUILeafData*> m_leaves;
    std::vector<IdType> m_openStack;
    std::vector<IdType> m_roots;

  public:
    void clear();

    UINodeState openContainer(
        const UILayoutConfig& layout = {}, const UIContainerStyle& style = {},
        std::string_view key = {}
    );
    void closeContainer();

    IdType addTextLeaf(const UILayoutConfig& layout, const UITextConfig& config);

    void draw();

    UINode* getNode(IdType id) { return m_nodes.get(id); }
    const UINode* getNode(IdType id) const { return m_nodes.get(id); }
    const IdIndexedVector<UINode>& getNodes() const { return m_nodes; }

  private:
    UIManager() = default;
    ~UIManager();

    UINodeState
    addNode(const UILayoutConfig& layout, const UIContainerStyle& style, std::string_view key = {});
};

}   // namespace Engine
