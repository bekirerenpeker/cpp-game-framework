#pragma once

#include <vector>

namespace Engine {

enum class GlDataType
{
    Byte,
    UnsignedByte,
    Short,
    UnsignedShort,
    Int,
    UnsignedInt,
    Float,
    Double,
    HalfFloat,
    Fixed,
};

struct GlLayoutElement
{
    GlDataType dataType;
    unsigned int count;
};

typedef std::vector<GlLayoutElement> GlLayout;

}   // namespace Engine
