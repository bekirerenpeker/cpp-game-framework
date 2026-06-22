#pragma once

#include "GlLayout.hpp"
#include "IGlObject.hpp"

namespace Engine {

class GlVertexArray : public IGlObject
{
  public:
    GlVertexArray();
    ~GlVertexArray();

    void bind() const override;
    void unbind() const override;

    void setLayout(const GlLayout& layout);

  private:
    unsigned int dataTypeSize(GlDataType dataType);
};

}   // namespace Engine
