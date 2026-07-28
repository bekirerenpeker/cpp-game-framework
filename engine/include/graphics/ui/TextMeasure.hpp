#pragma once

#include "graphics/text/Font.hpp"
#include "graphics/text/TextStyle.hpp"
#include "graphics/ui/ILeafMeasurer.hpp"
#include <string_view>
#include <vector>

namespace Engine {

namespace TextMeasure {

float lineStep(const Font& font, const TextStyle& style);
float lineBoxHeight(const Font& font, const TextStyle& style);
float heightForLines(const Font& font, const TextStyle& style, uint lineCount);

float measureRun(const Font& font, std::string_view text, const TextStyle& style);
float measureLongestWord(const Font& font, std::string_view text, const TextStyle& style);
float measureLongestLine(const Font& font, std::string_view text, const TextStyle& style);

uint wrap(
    const Font& font, std::string_view text, const TextStyle& style, float maxWidth,
    std::vector<LayoutLine>& lines
);

}   // namespace TextMeasure

}   // namespace Engine
