#pragma once

#include "utils/TypeAliases.hpp"

namespace Engine {

struct ImageData
{
    size_t width = 0, height = 0, depth = 0;
    byte* pixels = nullptr;

    ImageData() = default;
    ImageData(size_t w, size_t h, size_t d)
        : width(w), height(h), depth(d), pixels(new byte[w * h * d])
    {
    }
    ImageData(size_t w, size_t h, size_t d, byte* p) : width(w), height(h), depth(d), pixels(p) {}

    ~ImageData() { delete[] pixels; }

    ImageData(ImageData&& other) noexcept
        : width(other.width), height(other.height), depth(other.depth), pixels(other.pixels)
    {
        other.pixels = nullptr;
    }
    ImageData& operator=(ImageData&& other) noexcept
    {
        width = other.width, height = other.height, depth = other.depth;
        pixels = other.pixels, other.pixels = nullptr;
        return *this;
    }

    void setValueAt(size_t x, size_t y, size_t d, byte val)
    {
        if (x >= width || y >= height || d >= depth) return;
        pixels[(y * width * depth) + (x * depth) + d] = val;
    }
};

}   // namespace Engine
