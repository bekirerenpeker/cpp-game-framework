#pragma once

#include <vector>

namespace Engine {

enum class GlDataType
{
    Byte = 0x1400,
    UnsignedByte = 0x1401,
    Short = 0x1402,
    UnsignedShort = 0x1403,
    Int = 0x1404,
    UnsignedInt = 0x1405,
    Float = 0x1406,
    Double = 0x140A,
    HalfFloat = 0x140B,
    Fixed = 0x140C,
};

struct GlLayoutElement
{
    GlDataType dataType;
    unsigned int count;
};

typedef std::vector<GlLayoutElement> GlLayout;

}   // namespace Engine
