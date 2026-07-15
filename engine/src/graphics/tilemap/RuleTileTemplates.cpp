#include "graphics/tilemap/RuleTileTemplates.hpp"
#include <array>

namespace Engine {

namespace {

// TODO: fill in by hand. Index = the 4 orthogonal bits of the neighbor mask
// (bit0=N, bit2=E, bit4=S, bit6=W), reduced to bits 0..3 in that order.
std::array<RuleTileMapping, 16> g_sixteenTable {};

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
