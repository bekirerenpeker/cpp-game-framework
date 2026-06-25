#pragma once

#include "utils/TypeAliases.hpp"

namespace Engine {

struct CameraComponent
{
    IdType windowId;
    bool isPrimary = true;

    float orthoSize = 10.0f;
    float nearClip = -1.0f;
    float farClip = 1.0f;

    CameraComponent() = default;
};

}   // namespace Engine
