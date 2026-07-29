#pragma once

#include "graphics/text/Font.hpp"
#include "graphics/text/TextStyle.hpp"
#include "graphics/text/TextTags.hpp"
#include "graphics/ui/layout/ILeafMeasurer.hpp"
#include <string_view>
#include <vector>

namespace Engine {

// A slice of one span that sits on one line. offset.x is from the content box's
// left edge, offset.y is the line's baseline -- both ready to hand to
// TextRenderer::drawSpan without re-walking the string.
struct TextRun
{
    std::string_view text;
    int styleIndex = -1;
    Vec2 offset = VEC2_ZERO;
};

namespace TextMeasure {

const TextStyle& pickStyle(
    const TextSpan& span, const TextStyle& defaultStyle, const std::vector<TextStyle>& spanStyles
);

float lineStep(const Font& font, const TextStyle& style);
float lineBoxHeight(const Font& font, const TextStyle& style);

float measureRun(const Font& font, std::string_view text, const TextStyle& style);

LeafWidths measureSpanWidths(
    const Font& font, const std::vector<TextSpan>& spans, const TextStyle& defaultStyle,
    const std::vector<TextStyle>& spanStyles, bool wrapEnabled
);

float wrapSpans(
    const Font& font, const std::vector<TextSpan>& spans, const TextStyle& defaultStyle,
    const std::vector<TextStyle>& spanStyles, float maxWidth, bool fixedLineHeight,
    std::vector<TextRun>& runs, std::vector<LayoutLine>& lines
);

}   // namespace TextMeasure

}   // namespace Engine
