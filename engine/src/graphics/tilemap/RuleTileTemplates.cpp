#include "graphics/tilemap/RuleTileTemplates.hpp"
#include "core/logging/LoggerMacros.hpp"

namespace Engine {

namespace {

// A rule reduced to two byte filters so matching a neighbor mask is two ANDs and
// two compares: `mustSet` bits must all be present, `mustClear` bits must all be
// absent, and don't-care directions sit in neither.
struct RuleFilter
{
    uint8_t mustSet = 0;
    uint8_t mustClear = 0;
    uint8_t regionIndex = 0;
    int rotation = 0;
};

RuleFilter toFilter(const RuleTileRule& rule)
{
    RuleFilter f;
    f.regionIndex = rule.regionIndex;
    f.rotation = rule.rotation;
    const size_t n = rule.neighbors.size() < 8 ? rule.neighbors.size() : 8;
    for (size_t i = 0; i < n; i++) {
        const uint8_t bit = static_cast<uint8_t>(1u << i);
        if (rule.neighbors[i] == '1') f.mustSet |= bit;
        else if (rule.neighbors[i] == '0') f.mustClear |= bit;
    }
    return f;
}

}   // namespace

void RuleTileTemplateManager::createTemplate(
    const std::string& name, const std::vector<RuleTileRule>& rules
)
{
    m_compiled[name] = compile(rules);
}

const std::array<RuleTileMapping, 256>&
RuleTileTemplateManager::getTemplate(const std::string& name)
{
    auto it = m_compiled.find(name);
    if (it != m_compiled.end()) return it->second;

    if (name == "16-tile") return m_compiled.emplace(name, compile(builtin16Tile())).first->second;
    if (name == "47-tile") return m_compiled.emplace(name, compile(builtin47Tile())).first->second;

    LOG_WARNING("unknown rule-tile template '{}'; using empty (region 0) table", name);
    return m_compiled.emplace(name, std::array<RuleTileMapping, 256> {}).first->second;
}

int RuleTileTemplateManager::templateRegionCount(const std::string& name)
{
    const std::array<RuleTileMapping, 256>& table = getTemplate(name);
    int maxIndex = 0;
    for (const RuleTileMapping& m : table) {
        if (m.regionIndex > maxIndex) maxIndex = m.regionIndex;
    }
    return maxIndex + 1;
}

std::array<RuleTileMapping, 256>
RuleTileTemplateManager::compile(const std::vector<RuleTileRule>& rules)
{
    std::vector<RuleFilter> filters;
    filters.reserve(rules.size());
    for (const RuleTileRule& rule : rules) filters.push_back(toFilter(rule));

    std::array<RuleTileMapping, 256> table {};   // unmatched masks stay {0, 0}
    for (int mask = 0; mask < 256; mask++) {
        const uint8_t m = static_cast<uint8_t>(mask);
        for (const RuleFilter& f : filters) {
            if ((m & f.mustSet) == f.mustSet && (m & f.mustClear) == 0) {
                table[mask] = {f.regionIndex, f.rotation};
                break;   // first matching rule wins
            }
        }
    }
    return table;
}

std::vector<RuleTileRule> RuleTileTemplateManager::builtin16Tile()
{
    // Orthogonal-only autotile on the standard 4x4 sheet (bottom-left origin, so
    // region 0 is bottom-left and 15 is top-right). Only N/E/S/W matter, diagonals
    // are don't-care. The sheet is: top-left 3x3 = the filled blob island, right
    // column = vertical strip (top cap / middle / bottom cap), bottom row =
    // horizontal strip (left cap / middle / right cap) then the isolated tile at
    // bottom-right (region 3). Rule string is N,NE,E,SE,S,SW,W,NW.
    return {
        {"0x1x0x0x",  0, 0}, // E        (bottom row, left cap)
        {"0x1x0x1x",  1, 0}, // E W      (bottom row, horizontal middle)
        {"0x0x0x1x",  2, 0}, // W        (bottom row, right cap)
        {"0x0x0x0x",  3, 0}, // -        (isolated, bottom-right)
        {"1x1x0x0x",  4, 0}, // N E      (blob bottom-left corner)
        {"1x1x0x1x",  5, 0}, // N E W    (blob bottom edge)
        {"1x0x0x1x",  6, 0}, // N W      (blob bottom-right corner)
        {"1x0x0x0x",  7, 0}, // N        (vertical strip, bottom cap)
        {"1x1x1x0x",  8, 0}, // N E S    (blob left edge)
        {"1x1x1x1x",  9, 0}, // N E S W  (blob interior)
        {"1x0x1x1x", 10, 0}, // N S W    (blob right edge)
        {"1x0x1x0x", 11, 0}, // N S      (vertical strip, middle)
        {"0x1x1x0x", 12, 0}, // E S      (blob top-left corner)
        {"0x1x1x1x", 13, 0}, // E S W    (blob top edge)
        {"0x0x1x1x", 14, 0}, // S W      (blob top-right corner)
        {"0x0x1x0x", 15, 0}, // S        (vertical strip, top cap)
    };
}

std::vector<RuleTileRule> RuleTileTemplateManager::builtin47Tile()
{
    // Blob autotile: a diagonal only counts when both its adjacent orthogonals are
    // connected, giving 47 distinct configs. Regions are numbered in ascending
    // orthogonal-combo order (N + 2*E + 4*S + 8*W), then within a combo by the
    // relevant diagonals as a binary count (NE,SE,SW,NW low->high). Lay out 47 atlas
    // cells to match. Diagonals whose orthogonals aren't both present are 'x'.
    // Rule string is N,NE,E,SE,S,SW,W,NW.
    return {
        {"0x0x0x0x",  0, 0}, // -
        {"1x0x0x0x",  1, 0}, // N
        {"0x1x0x0x",  2, 0}, // E
        {"101x0x0x",  3, 0}, // N E, NE open
        {"111x0x0x",  4, 0}, // N E, NE filled
        {"0x0x1x0x",  5, 0}, // S
        {"1x0x1x0x",  6, 0}, // N S
        {"0x101x0x",  7, 0}, // E S, SE open
        {"0x111x0x",  8, 0}, // E S, SE filled
        {"10101x0x",  9, 0}, // N E S, NE open SE open
        {"11101x0x", 10, 0}, // N E S, NE filled SE open
        {"10111x0x", 11, 0}, // N E S, NE open SE filled
        {"11111x0x", 12, 0}, // N E S, NE filled SE filled
        {"0x0x0x1x", 13, 0}, // W
        {"1x0x0x10", 14, 0}, // N W, NW open
        {"1x0x0x11", 15, 0}, // N W, NW filled
        {"0x1x0x1x", 16, 0}, // E W
        {"101x0x10", 17, 0}, // N E W, NE open NW open
        {"111x0x10", 18, 0}, // N E W, NE filled NW open
        {"101x0x11", 19, 0}, // N E W, NE open NW filled
        {"111x0x11", 20, 0}, // N E W, NE filled NW filled
        {"0x0x101x", 21, 0}, // S W, SW open
        {"0x0x111x", 22, 0}, // S W, SW filled
        {"1x0x1010", 23, 0}, // N S W, SW open NW open
        {"1x0x1110", 24, 0}, // N S W, SW filled NW open
        {"1x0x1011", 25, 0}, // N S W, SW open NW filled
        {"1x0x1111", 26, 0}, // N S W, SW filled NW filled
        {"0x10101x", 27, 0}, // E S W, SE open SW open
        {"0x11101x", 28, 0}, // E S W, SE filled SW open
        {"0x10111x", 29, 0}, // E S W, SE open SW filled
        {"0x11111x", 30, 0}, // E S W, SE filled SW filled
        {"10101010", 31, 0}, // N E S W, no diagonals
        {"11101010", 32, 0}, // N E S W, NE
        {"10111010", 33, 0}, // N E S W, SE
        {"11111010", 34, 0}, // N E S W, NE SE
        {"10101110", 35, 0}, // N E S W, SW
        {"11101110", 36, 0}, // N E S W, NE SW
        {"10111110", 37, 0}, // N E S W, SE SW
        {"11111110", 38, 0}, // N E S W, NE SE SW
        {"10101011", 39, 0}, // N E S W, NW
        {"11101011", 40, 0}, // N E S W, NE NW
        {"10111011", 41, 0}, // N E S W, SE NW
        {"11111011", 42, 0}, // N E S W, NE SE NW
        {"10101111", 43, 0}, // N E S W, SW NW
        {"11101111", 44, 0}, // N E S W, NE SW NW
        {"10111111", 45, 0}, // N E S W, SE SW NW
        {"11111111", 46, 0}, // N E S W, all diagonals
    };
}

}   // namespace Engine
