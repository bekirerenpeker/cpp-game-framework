#pragma once

#include <cstdint>

namespace Engine {

typedef unsigned int uint;
typedef unsigned char byte;
//typedef unsigned long size_t;

typedef size_t IdType;
#define INVALID_ID (IdType)(0)
#define START_ID   (IdType)(1)

typedef uint64_t LayerMask;
typedef uint LayerId;
#define LAYER_NONE    (LayerMask)(0)
#define LAYER_ALL     (LayerMask)(~(LayerMask)(0))
#define LAYER_DEFAULT (LayerMask)(1)

}   // namespace Engine
