#pragma once

#include <utility>
#include <functional>
#include "math/Vec2.hpp"
#include "math/Vec3.hpp"
#include "math/Vec4.hpp"

struct pair_hash
{
    template<typename T1, typename T2> size_t operator()(const std::pair<T1, T2>& p) const
    {
        return (std::hash<T1> {}(p.first)) ^ (std::hash<T2> {}(p.second) << 1);
    }
};

namespace std {

template<> struct hash<Vec2>
{
    size_t operator()(const Vec2& k) const { return hash<float> {}(k.x) ^ hash<float> {}(k.y); }
};

template<> struct hash<Vec3>
{
    size_t operator()(const Vec3& k) const
    {
        return hash<float> {}(k.x) ^ hash<float> {}(k.y) ^ hash<float> {}(k.z);
    }
};

template<> struct hash<Vec4>
{
    size_t operator()(const Vec4& k) const
    {
        return hash<float> {}(k.x) ^ hash<float> {}(k.y) ^ hash<float> {}(k.z) ^
               hash<float> {}(k.w);
    }
};

}   // namespace std
