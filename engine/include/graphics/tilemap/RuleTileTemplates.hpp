#pragma once

#include <cstdint>

namespace Engine {

enum class RuleTileTemplate : uint8_t
{
    Sixteen,   // orthogonal-only, 4 neighbor bits -> 16 combos
    Full256,   // all 8 neighbor bits -> 256 combos
};

// One neighbor-bitmask resolves to: which of the rule tile's registered regions
// to draw, and how many quarter-turns to rotate its UVs (0..3 = 0/90/180/270deg),
// so one drawn region can cover multiple bitmask cases. Baked into the mesh at
// build time, not a runtime cost.
struct RuleTileMapping
{
    uint8_t regionIndex = 0;
    int rotation = 0;   // 0..3, clockwise quarter turns
};

// Number of distinct region entries a template expects to be given.
int ruleTileTemplateSize(RuleTileTemplate tmpl);

// neighborMask: 8-bit clockwise mask starting at N (up):
// bit0=N, bit1=NE, bit2=E, bit3=SE, bit4=S, bit5=SW, bit6=W, bit7=NW.
RuleTileMapping resolveRuleTileTemplate(RuleTileTemplate tmpl, uint8_t neighborMask);

}   // namespace Engine
