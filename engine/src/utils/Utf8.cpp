#include "utils/Utf8.hpp"

namespace Engine {

namespace Utf8 {

// Every malformed path advances index by at least one byte so a caller looping on
// index < size() can never spin forever on bad input.
uint32_t next(std::string_view text, size_t& index)
{
    if (index >= text.size()) return 0;

    unsigned char lead = (unsigned char)text[index];
    if (lead < 0x80) {
        index++;
        return lead;
    }

    int extraBytes = 0;
    uint32_t codepoint = 0;
    if ((lead & 0xE0) == 0xC0) {
        extraBytes = 1;
        codepoint = lead & 0x1F;
    } else if ((lead & 0xF0) == 0xE0) {
        extraBytes = 2;
        codepoint = lead & 0x0F;
    } else if ((lead & 0xF8) == 0xF0) {
        extraBytes = 3;
        codepoint = lead & 0x07;
    } else {
        // A bare continuation byte or an invalid 5/6-byte lead.
        index++;
        return REPLACEMENT_CODEPOINT;
    }

    // The sequence occupies index .. index + extraBytes, so it needs that last
    // byte to exist; a truncated tail at the end of the string does not.
    if (index + extraBytes >= text.size()) {
        index++;
        return REPLACEMENT_CODEPOINT;
    }

    for (int i = 1; i <= extraBytes; i++) {
        unsigned char continuation = (unsigned char)text[index + i];
        if ((continuation & 0xC0) != 0x80) {
            index++;
            return REPLACEMENT_CODEPOINT;
        }
        codepoint = (codepoint << 6) | (continuation & 0x3F);
    }

    index += extraBytes + 1;

    // Reject surrogates and out-of-range values so a bad sequence can't produce a
    // codepoint that would then be looked up as a real glyph.
    if (codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF))
        return REPLACEMENT_CODEPOINT;

    return codepoint;
}

size_t countCodepoints(std::string_view text)
{
    size_t count = 0;
    size_t index = 0;
    while (index < text.size()) {
        next(text, index);
        count++;
    }
    return count;
}

}   // namespace Utf8

}   // namespace Engine
