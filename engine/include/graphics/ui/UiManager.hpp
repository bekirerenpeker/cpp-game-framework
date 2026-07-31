#pragma once

#include "graphics/ui/UINode.hpp"
#include "utils/IdIndexedVector.hpp"
#include "utils/Singleton.hpp"

namespace Engine {

class UIManager : public Singleton<UIManager>
{
    friend class Singleton<UIManager>;

  private:
    IdIndexedVector<UINode> m_nodes;

  public:
    IdType addContainer(IdType parent = INVALID_ID, const UINodeStyle& style = {});

    UINode* getNode(IdType id) { return m_nodes.get(id); }
    const UINode* getNode(IdType id) const { return m_nodes.get(id); }
    const IdIndexedVector<UINode>& getNodes() const { return m_nodes; }

  private:
    UIManager() = default;
    ~UIManager() = default;
};

}   // namespace Engine
