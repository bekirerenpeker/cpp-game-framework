#include "graphics/text/FontCache.hpp"
#include "core/file_management/BinaryFile.hpp"
#include "core/file_management/File.hpp"
#include "core/file_management/ImageFile.hpp"
#include "core/file_management/JsonFile.hpp"
#include "core/logging/LoggerMacros.hpp"
#include <cctype>
#include <type_traits>
#include <vector>

namespace Engine {

namespace FontCache {

// Bump to invalidate every cached atlas at once after a change to the bake
// parameters or the on-disk schema. evictStale() collects the leftovers.
static constexpr uint32_t BAKER_VERSION = 1;

static constexpr uint64_t FNV_OFFSET_BASIS = 0xcbf29ce484222325ull;
static constexpr uint64_t FNV_PRIME = 0x100000001b3ull;

// FNV-1a rather than std::hash: the hash lands in a filename that has to stay
// valid across runs and builds, and std::hash offers no such stability.
static uint64_t hashBytes(const byte* data, size_t size, uint64_t hash)
{
    for (size_t i = 0; i < size; i++) {
        hash ^= (uint64_t)data[i];
        hash *= FNV_PRIME;
    }
    return hash;
}
static uint64_t hashString(const std::string& str, uint64_t hash)
{
    return hashBytes((const byte*)str.data(), str.size(), hash);
}
template<typename T> static uint64_t hashValue(const T& value, uint64_t hash)
{
    static_assert(std::is_trivial_v<T>, "only trivially copyable values can be hashed raw");
    return hashBytes((const byte*)&value, sizeof(T), hash);
}

fs::path getCacheFolder() { return fs::path(".cache") / "fonts"; }

static std::string toHex(uint64_t value)
{
    static constexpr char DIGITS[] = "0123456789abcdef";
    std::string out(16, '0');
    for (int i = 15; i >= 0; i--) {
        out[i] = DIGITS[value & 0xf];
        value >>= 4;
    }
    return out;
}

static fs::path entryPath(const FontCacheKey& key, const char* extension)
{
    return getCacheFolder() / (key.stem + "-" + toHex(key.settingsHash) + extension);
}

static uint64_t hashFileContents(const fs::path& path)
{
    BinaryFile file(path);
    if (!file.isValid() || !file.open(std::ios::in | std::ios::binary)) {
        LOG_WARNING("couldn't open {} to hash its contents", path);
        return 0;
    }

    uint64_t hash = FNV_OFFSET_BASIS;
    std::vector<byte> buffer(64 * 1024);
    size_t remaining = file.getSize();
    while (remaining > 0) {
        size_t chunk = remaining < buffer.size() ? remaining : buffer.size();
        if (!file.readBytes(buffer.data(), chunk)) break;
        hash = hashBytes(buffer.data(), chunk, hash);
        remaining -= chunk;
    }

    file.close();
    return hash;
}

bool buildKey(const fs::path& fontPath, const FontBakeSettings& settings, FontCacheKey& outKey)
{
    if (!FileManager::get().doesPathExist(fontPath)) {
        LOG_ERROR("font file {} doesn't exist", fontPath);
        return false;
    }

    std::error_code ec;
    outKey.sourcePath = fontPath;
    outKey.stem = fontPath.stem().string();
    outKey.sourceSize = fs::file_size(fontPath, ec);
    if (ec) outKey.sourceSize = 0;
    outKey.sourceMtime = fs::last_write_time(fontPath, ec).time_since_epoch().count();
    if (ec) outKey.sourceMtime = 0;

    uint64_t hash = FNV_OFFSET_BASIS;
    hash = hashString(fontPath.filename().string(), hash);
    hash = hashValue(settings.atlasType, hash);
    hash = hashValue(settings.charset, hash);
    hash = hashValue(settings.emPixelSize, hash);
    // Only the distance-field types are affected by the range, but folding it in
    // unconditionally keeps the key derivation branch-free.
    hash = hashValue(settings.distanceRangePixels, hash);
    hash = hashValue(BAKER_VERSION, hash);
    outKey.settingsHash = hash;

    return true;
}

static bool matchesSettings(const JsonFile::json& bake, const FontBakeSettings& settings)
{
    return bake.value("atlasType", -1) == (int)settings.atlasType &&
           bake.value("charset", -1) == (int)settings.charset &&
           bake.value("emPixelSize", 0u) == settings.emPixelSize &&
           bake.value("distanceRangePixels", 0u) == settings.distanceRangePixels &&
           bake.value("bakerVersion", 0u) == BAKER_VERSION;
}

bool load(const FontCacheKey& key, const FontBakeSettings& settings, FontData& outData)
{
    fs::path jsonPath = entryPath(key, ".json");
    fs::path imagePath = entryPath(key, ".png");
    if (!FileManager::get().doesPathExist(jsonPath) || !FileManager::get().doesPathExist(imagePath))
        return false;

    JsonFile metaFile(jsonPath);
    if (!metaFile.isValid()) return false;
    JsonFile::json& meta = metaFile.getData();
    if (!meta.contains("bake") || !meta.contains("source") || !meta.contains("glyphs")) {
        LOG_WARNING("font cache entry {} is malformed; rebaking", jsonPath);
        return false;
    }

    // Guards against a settings hash collision or a hand-edited cache entry.
    if (!matchesSettings(meta["bake"], settings)) {
        LOG_WARNING("font cache entry {} doesn't match the requested settings; rebaking", jsonPath);
        return false;
    }

    const JsonFile::json& source = meta["source"];
    bool statMatches = source.value("size", (uintmax_t)0) == key.sourceSize &&
                       source.value("mtime", (int64_t)0) == key.sourceMtime;
    if (!statMatches) {
        // A copy or a git checkout moves the mtime without touching the bytes, so
        // fall back to the contents before paying for a rebake.
        uint64_t contentHash = hashFileContents(key.sourcePath);
        if (contentHash == 0 || source.value("contentHash", (uint64_t)0) != contentHash)
            return false;

        meta["source"]["size"] = key.sourceSize;
        meta["source"]["mtime"] = key.sourceMtime;
        metaFile.save();
        LOG_INFO("{} changed timestamp but not contents; keeping the cached atlas", key.sourcePath);
    }

    int desiredChannels = settings.atlasType == FontAtlasType::Mtsdf ? 4 : 1;
    ImageFile atlasFile(imagePath);
    if (!atlasFile.loadImage(desiredChannels)) {
        LOG_WARNING("couldn't read the cached atlas {}; rebaking", imagePath);
        return false;
    }
    outData.atlasImage = std::move(atlasFile.getImageData());

    outData.distanceRange = meta["bake"].value("bakedDistanceRange", 0.0f);

    const JsonFile::json& metrics = meta["metrics"];
    outData.metrics.emSize = metrics.value("emSize", 1.0f);
    outData.metrics.lineHeight = metrics.value("lineHeight", 1.0f);
    outData.metrics.ascender = metrics.value("ascender", 0.0f);
    outData.metrics.descender = metrics.value("descender", 0.0f);
    outData.metrics.underlineY = metrics.value("underlineY", 0.0f);
    outData.metrics.underlineThickness = metrics.value("underlineThickness", 0.0f);

    outData.glyphs.clear();
    outData.glyphs.reserve(meta["glyphs"].size());
    for (const JsonFile::json& entry : meta["glyphs"]) {
        Glyph glyph;
        glyph.codepoint = entry.value("cp", 0u);
        glyph.advance = entry.value("adv", 0.0f);
        const JsonFile::json& quad = entry["quad"];
        const JsonFile::json& uv = entry["uv"];
        if (quad.size() == 4 && uv.size() == 4) {
            glyph.quadMin = Vec2(quad[0].get<float>(), quad[1].get<float>());
            glyph.quadMax = Vec2(quad[2].get<float>(), quad[3].get<float>());
            glyph.uvMin = Vec2(uv[0].get<float>(), uv[1].get<float>());
            glyph.uvMax = Vec2(uv[2].get<float>(), uv[3].get<float>());
        }
        outData.glyphs.push_back(glyph);
    }

    outData.kerning.clear();
    if (meta.contains("kerning")) {
        outData.kerning.reserve(meta["kerning"].size());
        for (const JsonFile::json& entry : meta["kerning"]) {
            FontKerningPair pair;
            pair.left = entry.value("l", 0u);
            pair.right = entry.value("r", 0u);
            pair.advance = entry.value("adv", 0.0f);
            outData.kerning.push_back(pair);
        }
    }

    return true;
}

// Renames over the destination, replacing it. Retries after an explicit remove
// because a rename onto an existing file is not guaranteed to succeed on Windows.
static bool replaceFile(const fs::path& from, const fs::path& to)
{
    std::error_code ec;
    fs::rename(from, to, ec);
    if (!ec) return true;

    fs::remove(to, ec);
    ec.clear();
    fs::rename(from, to, ec);
    return !ec;
}

bool store(const FontCacheKey& key, const FontBakeSettings& settings, const FontData& data)
{
    // Checked first because createFolder warns when the folder already exists, and
    // every font after the first would trip it.
    if (!FileManager::get().doesPathExist(getCacheFolder()) &&
        !FileManager::get().createFolder(getCacheFolder())) {
        LOG_ERROR("couldn't create the font cache folder {}", getCacheFolder());
        return false;
    }

    // The temp names keep the real extension last so createImageFile still
    // recognises the format and so a half-written file never sits at the final path.
    fs::path imageTemp = entryPath(key, ".tmp.png");
    fs::path jsonTemp = entryPath(key, ".tmp.json");
    fs::path imagePath = entryPath(key, ".png");
    fs::path jsonPath = entryPath(key, ".json");

    std::error_code ec;
    fs::remove(imageTemp, ec);
    fs::remove(jsonTemp, ec);

    if (!FileManager::get().createImageFile(imageTemp, data.atlasImage)) {
        LOG_ERROR("couldn't write the font atlas {}", imageTemp);
        return false;
    }

    if (!FileManager::get().createFile(jsonTemp)) {
        LOG_ERROR("couldn't create the font metrics file {}", jsonTemp);
        fs::remove(imageTemp, ec);
        return false;
    }

    JsonFile metaFile(jsonTemp);
    JsonFile::json& meta = metaFile.getData();

    meta["source"]["path"] = key.sourcePath.generic_string();
    meta["source"]["size"] = key.sourceSize;
    meta["source"]["mtime"] = key.sourceMtime;
    meta["source"]["contentHash"] = hashFileContents(key.sourcePath);

    meta["bake"]["atlasType"] = (int)settings.atlasType;
    meta["bake"]["charset"] = (int)settings.charset;
    meta["bake"]["emPixelSize"] = settings.emPixelSize;
    meta["bake"]["distanceRangePixels"] = settings.distanceRangePixels;
    meta["bake"]["bakedDistanceRange"] = data.distanceRange;
    meta["bake"]["bakerVersion"] = BAKER_VERSION;

    meta["atlas"]["width"] = data.atlasImage.width;
    meta["atlas"]["height"] = data.atlasImage.height;
    meta["atlas"]["channels"] = data.atlasImage.depth;

    meta["metrics"]["emSize"] = data.metrics.emSize;
    meta["metrics"]["lineHeight"] = data.metrics.lineHeight;
    meta["metrics"]["ascender"] = data.metrics.ascender;
    meta["metrics"]["descender"] = data.metrics.descender;
    meta["metrics"]["underlineY"] = data.metrics.underlineY;
    meta["metrics"]["underlineThickness"] = data.metrics.underlineThickness;

    meta["glyphs"] = JsonFile::json::array();
    for (const Glyph& glyph : data.glyphs) {
        JsonFile::json entry;
        entry["cp"] = glyph.codepoint;
        entry["adv"] = glyph.advance;
        entry["quad"] = {glyph.quadMin.x, glyph.quadMin.y, glyph.quadMax.x, glyph.quadMax.y};
        entry["uv"] = {glyph.uvMin.x, glyph.uvMin.y, glyph.uvMax.x, glyph.uvMax.y};
        meta["glyphs"].push_back(entry);
    }

    meta["kerning"] = JsonFile::json::array();
    for (const FontKerningPair& pair : data.kerning) {
        JsonFile::json entry;
        entry["l"] = pair.left;
        entry["r"] = pair.right;
        entry["adv"] = pair.advance;
        meta["kerning"].push_back(entry);
    }

    if (!metaFile.save()) {
        LOG_ERROR("couldn't write the font metrics {}", jsonTemp);
        fs::remove(imageTemp, ec);
        fs::remove(jsonTemp, ec);
        return false;
    }

    if (!replaceFile(imageTemp, imagePath) || !replaceFile(jsonTemp, jsonPath)) {
        LOG_ERROR("couldn't move the baked font cache entry into place at {}", jsonPath);
        fs::remove(imageTemp, ec);
        fs::remove(jsonTemp, ec);
        return false;
    }

    return true;
}

// Refuses anything that isn't a "<stem>-<16 hex>.json/.png" inside the cache
// folder. ALLOW_DELETE_FROM_DISK is on for the whole engine, so this is what keeps
// a mistake here from reaching a real file.
static bool isEvictablePath(const fs::path& path, const std::string& stem)
{
    if (path.parent_path() != getCacheFolder()) return false;

    std::string extension = path.extension().string();
    if (extension != ".json" && extension != ".png") return false;

    std::string name = path.stem().string();
    std::string prefix = stem + "-";
    if (name.size() != prefix.size() + 16 || name.compare(0, prefix.size(), prefix) != 0)
        return false;

    for (size_t i = prefix.size(); i < name.size(); i++) {
        if (!std::isxdigit((unsigned char)name[i])) return false;
    }
    return true;
}

static void deleteEntry(const fs::path& path)
{
    if (!FileManager::get().doesPathExist(path)) return;
    File file(path);
    if (!file.deleteFromDisk()) LOG_WARNING("couldn't evict the font cache entry {}", path);
}

void evictStale(const FontCacheKey& key)
{
    if (!FileManager::get().doesPathExist(getCacheFolder())) return;

    uint64_t currentContentHash = hashFileContents(key.sourcePath);
    fs::path currentJson = entryPath(key, ".json");

    std::error_code ec;
    std::vector<fs::path> doomed;
    for (const fs::directory_entry& entry : fs::directory_iterator(getCacheFolder(), ec)) {
        fs::path path = entry.path();
        if (path.extension() != ".json") continue;
        if (path == currentJson) continue;
        if (!isEvictablePath(path, key.stem)) continue;

        JsonFile metaFile(path);
        const JsonFile::json& meta = metaFile.getData();
        if (!meta.contains("bake") || !meta.contains("source")) continue;

        // Variants of the same font under different settings are legitimate and
        // must survive -- only drop the ones baked from different bytes, or from a
        // baker version whose output we no longer understand.
        bool wrongVersion = meta["bake"].value("bakerVersion", 0u) != BAKER_VERSION;
        bool wrongContents = meta["source"].value("contentHash", (uint64_t)0) != currentContentHash;
        if (wrongVersion || wrongContents) doomed.push_back(path);
    }

    for (const fs::path& path : doomed) {
        deleteEntry(path);
        deleteEntry(fs::path(path).replace_extension(".png"));
        LOG_INFO("evicted the stale font cache entry {}", path.stem());
    }
}

}   // namespace FontCache

}   // namespace Engine
