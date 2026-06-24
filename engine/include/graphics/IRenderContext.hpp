#pragma once

#include "graphics/gl_wrappers/GlVertexArray.hpp"

namespace Engine {

class Renderer;

class IRenderContext
{
    friend class Renderer;

  protected:
    GlVertexArray* m_vertexArray = nullptr;

  public:
    IRenderContext() = default;
    virtual ~IRenderContext() { delete m_vertexArray; }

  protected:
    virtual void bindRenderContext() = 0;
    virtual void unbindRenderContext() = 0;
    virtual int getRenderContextWidth() = 0;
    virtual int getRenderContextHeight() = 0;
};

}   // namespace Engine
