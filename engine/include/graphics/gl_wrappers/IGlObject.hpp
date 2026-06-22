#pragma once

namespace Engine {

class IGlObject
{
  protected:
    unsigned int m_GlId = -1;

  public:
    virtual ~IGlObject() = default;

    virtual void bind() const = 0;
    virtual void unbind() const = 0;

    const unsigned int getGlId() const { return m_GlId; }
};

}   // namespace Engine
