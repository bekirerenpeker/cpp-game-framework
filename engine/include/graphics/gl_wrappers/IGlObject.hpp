#pragma once

#include <utility>

namespace Engine {

class IGlObject
{
  protected:
    unsigned int m_GlId = -1;

  public:
    virtual ~IGlObject() = default;

    // GL objects own a single GL name and must not be copied (a copy would
    // double-delete the name). They are move-only: moving swaps the ids so the
    // moved-from object deletes whatever id it ends up holding via its own
    // derived destructor.
    IGlObject(const IGlObject&) = delete;
    IGlObject& operator=(const IGlObject&) = delete;

    IGlObject(IGlObject&& other) noexcept { std::swap(m_GlId, other.m_GlId); }
    IGlObject& operator=(IGlObject&& other) noexcept
    {
        std::swap(m_GlId, other.m_GlId);
        return *this;
    }

    virtual void bind() const = 0;
    virtual void unbind() const = 0;

    const unsigned int getGlId() const { return m_GlId; }

  protected:
    IGlObject() = default;
};

}   // namespace Engine
