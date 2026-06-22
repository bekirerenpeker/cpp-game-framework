#pragma once

#include "IGlObject.hpp"

namespace Engine {

enum class GlBufferType
{
    VertexBuffer = 34962,
    IndexBuffer = 34963,
};

enum class GlBufferUsage
{
    StaticDraw = 35044,
    DynamicDraw = 35048,
};

class GlBuffer : public IGlObject
{
  private:
    GlBufferType m_type;

  public:
    GlBuffer(GlBufferType type);
    ~GlBuffer() override;

    void bind() const override;
    void unbind() const override;

    void setData(long size, const void* data, GlBufferUsage usage);
};

}   // namespace Engine
