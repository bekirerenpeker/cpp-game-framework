#pragma once

#include "utils/TypeAliases.hpp"
#include "stb/stb_image.h"

namespace Engine {

struct ImageData
{
    size_t width = 0, height = 0, depth = 0;
    byte* pixels = nullptr;
    bool isStbiAllocated = false;

    ImageData() = default;
    ImageData(size_t w, size_t h, size_t d)
        : width(w), height(h), depth(d), pixels(new byte[w * h * d]), isStbiAllocated(false)
    {
    }
    ImageData(size_t w, size_t h, size_t d, byte* p, bool isStbi = false)
        : width(w), height(h), depth(d), pixels(p), isStbiAllocated(isStbi)
    {
    }

    ~ImageData()
    {
        if (isStbiAllocated) stbi_image_free(pixels);
        else delete[] pixels;
    }

    ImageData(ImageData&& other) noexcept
        : width(other.width),
          height(other.height),
          depth(other.depth),
          pixels(other.pixels),
          isStbiAllocated(other.isStbiAllocated)
    {
        other.pixels = nullptr;
        other.isStbiAllocated = false;
    }
    ImageData& operator=(ImageData&& other) noexcept
    {
        width = other.width, height = other.height, depth = other.depth;
        pixels = other.pixels, other.pixels = nullptr;
        isStbiAllocated = other.isStbiAllocated, other.isStbiAllocated = false;
        return *this;
    }

    void setValueAt(size_t x, size_t y, size_t d, byte val)
    {
        if (x >= width || y >= height || d >= depth) return;
        pixels[(y * width * depth) + (x * depth) + d] = val;
    }
};

}   // namespace Engine
