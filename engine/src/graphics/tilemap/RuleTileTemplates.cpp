#include "graphics/tilemap/RuleTileTemplates.hpp"
#include <array>

namespace Engine {

namespace {

std::array<RuleTileMapping, 16> g_sixteenTable {
    {{3, 0},
     {7, 0},
     {0, 0},
     {4, 0},
     {15, 0},
     {11, 0},
     {12, 0},
     {8, 0},
     {2, 0},
     {6, 0},
     {1, 0},
     {5, 0},
     {14, 0},
     {10, 0},
     {13, 0},
     {9, 0}}
};

// TODO: fill in by hand. Index = the full 8-bit clockwise neighbor mask.
std::array<RuleTileMapping, 256> g_full256Table {};

}   // namespace

int ruleTileTemplateSize(RuleTileTemplate tmpl)
{
    switch (tmpl) {
    case RuleTileTemplate::Sixteen: return 16;
    case RuleTileTemplate::Full256: return 256;
    }
    return 0;
}

RuleTileMapping resolveRuleTileTemplate(RuleTileTemplate tmpl, uint8_t neighborMask)
{
    switch (tmpl) {
    case RuleTileTemplate::Sixteen: {
        uint8_t idx = (neighborMask & 1) | (((neighborMask >> 2) & 1) << 1) |
                      (((neighborMask >> 4) & 1) << 2) | (((neighborMask >> 6) & 1) << 3);
        return g_sixteenTable[idx];
    }
    case RuleTileTemplate::Full256: return g_full256Table[neighborMask];
    }
    return {};
}

}   // namespace Engine
