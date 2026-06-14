#pragma once

#include "utils/Singleton.hpp"
#include "utils/TypeAliases.hpp"

namespace Engine {

class TypeRegistery : public Singleton<TypeRegistery>
{
    friend class Singleton<TypeRegistery>;

  private:
    IdType m_nextID = START_ID;

  public:
    // assigns a unique ID to each type ( very usefull )
    template<typename T> IdType getTypeId()
    {
        static const IdType id = m_nextID++;
        return id;
    }

  private:
    TypeRegistery() = default;
    ~TypeRegistery() = default;
};

}   // namespace Engine
