#pragma once

#include "IUILeafData.hpp"
#include "graphics/text/TextLayout.hpp"
#include <string_view>

namespace Engine {

class UITextLeafData : public IUILeafData
{
  private:
    TextBlock m_block;

  public:
    UITextLeafData(
        const Font* font, std::string_view text, const TextStyle& textStyle,
        const TextAlignment& alignment = {}, TextOverflow overflow = TextOverflow::Visible
    );
    ~UITextLeafData() = default;

    TextBlock& getBlock() { return m_block; }
    const TextBlock& getBlock() const { return m_block; }

    UILeafWidths measureWidths() override;
    float measureHeight(float contentWidth) override;
    void draw(Vec2 pos, Vec2 size) override;
};

}   // namespace Engine
