#pragma once

#include "utils/Singleton.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Engine {

// One authored rule. `neighbors` is an 8-char string, starting at N (top) and
// going clockwise (N, NE, E, SE, S, SW, W, NW): '1' = must connect, '0' = must
// not connect, any other char (e.g. 'x') = don't care. On match, the tile draws
// `regionIndex` rotated `rotation` quarter-turns.
struct RuleTileRule
{
    std::string neighbors;   // 8 chars, e.g. "1x1xxxxx"
    uint8_t regionIndex = 0;
    int rotation = 0;   // 0..3, clockwise quarter turns
};

// The compiled per-mask result: which region to draw and how many quarter-turns
// to rotate its UVs. Baked into the mesh at build time, not a runtime cost.
struct RuleTileMapping
{
    uint8_t regionIndex = 0;
    int rotation = 0;
};

// Compiles authored rule lists into 256-entry lookup tables (one per neighbor
// bitmask) and caches them by template name. Built-in templates are generated
// lazily on first request, so only templates actually used cost anything.
class RuleTileTemplateManager : public Singleton<RuleTileTemplateManager>
{
    friend class Singleton<RuleTileTemplateManager>;

  private:
    // Compiled 256-entry tables keyed by template name. unordered_map guarantees
    // element pointers stay valid across inserts, so Tileset can cache a pointer
    // straight into this map. Entries are never erased.
    std::unordered_map<std::string, std::array<RuleTileMapping, 256>> m_compiled;

  public:
    // Registers and compiles a custom template. Overwrites any existing template
    // of the same name.
    void createTemplate(const std::string& name, const std::vector<RuleTileRule>& rules);
    // Compiled table for `name`. Lazily generates the built-in "16-tile" template
    // on first request; an unknown name gets an all-{0,0} table plus a warning.
    const std::array<RuleTileMapping, 256>& getTemplate(const std::string& name);
    // Distinct region count a template references (max regionIndex + 1). Used for
    // the variant-count sanity warning when a rule tile is created.
    int templateRegionCount(const std::string& name);

  private:
    RuleTileTemplateManager() = default;
    ~RuleTileTemplateManager() = default;
    static std::array<RuleTileMapping, 256> compile(const std::vector<RuleTileRule>& rules);
    static std::vector<RuleTileRule> builtin16Tile();
    static std::vector<RuleTileRule> builtin47Tile();
};

}   // namespace Engine
