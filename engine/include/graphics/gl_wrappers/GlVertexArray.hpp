#pragma once

#include "GlLayout.hpp"
#include "IGlObject.hpp"

namespace Engine {

class GlVertexArray : public IGlObject
{
  public:
    GlVertexArray();
    ~GlVertexArray();

    GlVertexArray(GlVertexArray&&) = default;
    GlVertexArray& operator=(GlVertexArray&&) = default;

    void bind() const override;
    void unbind() const override;

    void setLayout(const GlLayout& layout);

  private:
    unsigned int dataTypeSize(GlDataType dataType) const;
    unsigned int glDataTypeVal(GlDataType dataType) const;
};

}   // namespace Engine
