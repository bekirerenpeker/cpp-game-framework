#pragma once

#include <cstdint>
#include <string_view>

namespace Engine {

namespace Utf8 {

constexpr uint32_t REPLACEMENT_CODEPOINT = 0xFFFD;

uint32_t next(std::string_view text, size_t& index);
size_t countCodepoints(std::string_view text);

}   // namespace Utf8

}   // namespace Engine
