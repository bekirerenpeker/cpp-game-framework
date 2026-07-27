#include "graphics/text/TextTags.hpp"

namespace Engine {

namespace TextTags {

// Checked before isTagAt everywhere, which is what keeps "http://x" intact: only a
// doubled prefix immediately before the tag letter is an escape, so "//x" stays
// ordinary text.
bool isEscapedTagAt(std::string_view text, size_t index)
{
    return index + 2 < text.size() && text[index] == TAG_PREFIX && text[index + 1] == TAG_PREFIX &&
           text[index + 2] == TAG_STYLE;
}

bool isTagAt(std::string_view text, size_t index)
{
    return index + 1 < text.size() && text[index] == TAG_PREFIX && text[index + 1] == TAG_STYLE;
}

bool parse(std::string_view text, std::vector<TextSpan>& outSpans)
{
    outSpans.clear();
    if (text.empty()) return true;

    size_t spanStart = 0;
    int styleIndex = -1;
    int nextStyleIndex = 0;
    int tagCount = 0;

    size_t i = 0;
    while (i < text.size()) {
        if (isEscapedTagAt(text, i)) {
            i += 3;
            continue;
        }
        if (!isTagAt(text, i)) {
            i++;
            continue;
        }

        outSpans.push_back({text.substr(spanStart, i - spanStart), styleIndex});
        tagCount++;

        // Same token opens and closes, so a tag just toggles: entering a styled
        // span consumes the next style slot, leaving one returns to the default.
        if (styleIndex < 0) styleIndex = nextStyleIndex++;
        else styleIndex = -1;

        i += 2;
        spanStart = i;
    }

    outSpans.push_back({text.substr(spanStart), styleIndex});

    return tagCount % 2 == 0;
}

}   // namespace TextTags

}   // namespace Engine
