#pragma once

#include "IGlObject.hpp"

namespace Engine {

enum class GlBufferType
{
    VertexBuffer,
    IndexBuffer,
};

enum class GlBufferUsage
{
    StaticDraw,
    DynamicDraw,
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
    void setSubData(long size, const void* data);

  private:
    unsigned int glBufferTypeVal(GlBufferType type) const;
    unsigned int glBufferUsageVal(GlBufferUsage usage) const;
};

}   // namespace Engine
