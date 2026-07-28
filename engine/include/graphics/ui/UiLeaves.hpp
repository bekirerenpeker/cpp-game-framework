#pragma once

#include "graphics/text/Font.hpp"
#include "graphics/text/TextStyle.hpp"
#include "graphics/ui/ILeafMeasurer.hpp"
#include <string>
#include <string_view>

namespace Engine {

class GlTexture;

class TextLeaf : public ILeafMeasurer
{
  private:
    const Font* m_font = nullptr;
    std::string m_text;
    TextStyle m_style;
    bool m_wrapEnabled = true;

  public:
    void set(const Font* font, std::string_view text, const TextStyle& style, bool wrapEnabled);

    const Font* getFont() const { return m_font; }
    const std::string& getText() const { return m_text; }
    const TextStyle& getStyle() const { return m_style; }

    LeafWidths measureWidths() override;
    float measureHeight(float contentWidth, std::vector<LayoutLine>& lines) override;
};

class ImageLeaf : public ILeafMeasurer
{
  private:
    const GlTexture* m_texture = nullptr;
    Vec2 m_nativeSize = VEC2_ZERO;
    float m_aspectRatio = 0.0f;

  public:
    void set(const GlTexture* texture, Vec2 nativeSize, float aspectRatio);

    const GlTexture* getTexture() const { return m_texture; }
    Vec2 getNativeSize() const { return m_nativeSize; }

    LeafWidths measureWidths() override;
    float measureHeight(float contentWidth, std::vector<LayoutLine>& lines) override;
};

}   // namespace Engine
