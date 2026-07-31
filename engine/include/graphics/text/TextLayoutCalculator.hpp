#pragma once

#include "graphics/text/TextLayout.hpp"
#include "utils/Singleton.hpp"

namespace Engine {

class TextLayoutCalculator : public Singleton<TextLayoutCalculator>
{
    friend class Singleton<TextLayoutCalculator>;

  public:
    float calculate(TextBlock& block, float availableWidth);
    TextBlockWidths measureMinMaxWidth(TextBlock& block);

  private:
    TextLayoutCalculator() = default;
    ~TextLayoutCalculator() = default;

    static void applyHorizontalAlign(TextBlock& block, float contentWidth);
};

}   // namespace Engine
