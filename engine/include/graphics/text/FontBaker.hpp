#pragma once

#include "graphics/text/Font.hpp"

namespace Engine {

namespace FontBaker {

bool isAvailable();

bool bake(const fs::path& fontPath, const FontBakeSettings& settings, FontData& outData);

}   // namespace FontBaker

}   // namespace Engine
