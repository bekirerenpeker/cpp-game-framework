#pragma once

#include "utils/IdIndexedVector.hpp"

namespace Engine {

class IResource : public IHasId
{
  public:
    IResource() = default;
    virtual ~IResource() = default;
};

}   // namespace Engine
