#pragma once

namespace Engine {

template<typename T> class Singleton
{
  public:
    static T& get()
    {
        static T instance;
        return instance;
    }

  protected:
    Singleton() = default;
    ~Singleton() = default;
};

}   // namespace Engine
