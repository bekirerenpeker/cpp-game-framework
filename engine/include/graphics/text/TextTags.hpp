#pragma once

#include <string_view>
#include <vector>

namespace Engine {

// A stretch of text sharing one style. styleIndex is -1 for text outside any tag
// pair, otherwise the running index into the caller's style list. text is a view
// into the original string and still contains any //s escapes, which the glyph
// walk resolves.
struct TextSpan
{
    std::string_view text;
    int styleIndex = -1;
};

namespace TextTags {

constexpr char TAG_PREFIX = '/';
constexpr char TAG_STYLE = 's';

// parse returns false when the tag count is odd, i.e. a span was left open and
// runs to the end of the string. It only reports; the caller decides whether to
// warn, so a per-frame parse of the same string can't flood the log.

bool parse(std::string_view text, std::vector<TextSpan>& outSpans);

bool isTagAt(std::string_view text, size_t index);
bool isEscapedTagAt(std::string_view text, size_t index);

}   // namespace TextTags

}   // namespace Engine
