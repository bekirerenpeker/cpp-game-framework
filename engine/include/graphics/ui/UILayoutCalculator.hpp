#pragma once

#include "utils/Singleton.hpp"

namespace Engine {

class UILayoutCalculator : public Singleton<UILayoutCalculator>
{
    friend class Singleton<UILayoutCalculator>;

  public:
  private:
    UILayoutCalculator() = default;
    ~UILayoutCalculator() = default;
};

}   // namespace Engine
