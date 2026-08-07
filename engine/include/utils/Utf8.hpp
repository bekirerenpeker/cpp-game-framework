#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace Engine {

namespace Utf8 {

constexpr uint32_t REPLACEMENT_CODEPOINT = 0xFFFD;

uint32_t next(std::string_view text, size_t& index);
// Steps index back onto the lead byte of the codepoint before it and returns that
// codepoint. The mirror of next, and what a caret needs: a byte-wise step back would
// land inside a multi-byte sequence and split it.
uint32_t prev(std::string_view text, size_t& index);
void encode(uint32_t codepoint, std::string& out);
size_t countCodepoints(std::string_view text);

}   // namespace Utf8

}   // namespace Engine
