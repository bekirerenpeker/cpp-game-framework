#pragma once

#include "graphics/text/Font.hpp"
#include <string>

namespace Engine {

// Identifies one baked variant on disk. The hash covers the bake settings and the
// source filename but deliberately NOT the font's contents -- that keeps the key
// computable without reading the .ttf, and makes a rebake overwrite its own entry
// instead of orphaning it.
struct FontCacheKey
{
    fs::path sourcePath;
    std::string stem;
    uint64_t settingsHash = 0;
    uintmax_t sourceSize = 0;
    int64_t sourceMtime = 0;
};

namespace FontCache {

bool buildKey(const fs::path& fontPath, const FontBakeSettings& settings, FontCacheKey& outKey);

bool load(const FontCacheKey& key, const FontBakeSettings& settings, FontData& outData);
bool store(const FontCacheKey& key, const FontBakeSettings& settings, const FontData& data);

void evictStale(const FontCacheKey& key);

fs::path getCacheFolder();

}   // namespace FontCache

}   // namespace Engine
