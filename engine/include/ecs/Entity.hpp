#pragma once

#include <cstdint>

namespace Engine {

// lower 20 bits are core id, higher 12 bits are generation
typedef uint32_t Entity;
#define NULL_ENTITY UINT32_MAX

inline uint32_t getEntityId(Entity entity) { return entity & 0xFFFFF; }
inline uint16_t getEntityGen(Entity entity) { return entity >> 20; }
inline Entity constructEntity(uint32_t id, uint16_t gen) { return (gen << 20) | id; }

}   // namespace Engine
