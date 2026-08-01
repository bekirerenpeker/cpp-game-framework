#pragma once

#include "IUILeafData.hpp"
#include "graphics/text/TextLayout.hpp"
#include <string_view>
#include <vector>

namespace Engine {

// Everything a text leaf needs. spanStyles is index-matched to the "/s" tags in
// text, and a short list falls back to style rather than being an error.
struct UITextConfig
{
    const Font* font = nullptr;
    std::string_view text;
    TextStyle style;
    std::vector<TextStyle> spanStyles;
    TextAlignment alignment;
    TextOverflow overflow = TextOverflow::Visible;
    bool wrapEnabled = true;
    bool fixedLineHeight = false;
};

class UITextLeafData : public IUILeafData
{
  private:
    TextBlock m_block;

  public:
    UITextLeafData(const UITextConfig& config);
    ~UITextLeafData() = default;

    TextBlock& getBlock() { return m_block; }
    const TextBlock& getBlock() const { return m_block; }

    UILeafWidths measureWidths() override;
    float measureHeight(float contentWidth) override;
    void draw(Vec2 drawPos, Vec2 size) override;
};

}   // namespace Engine
