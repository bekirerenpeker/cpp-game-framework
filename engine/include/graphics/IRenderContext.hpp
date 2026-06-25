#pragma once

#include "graphics/gl_wrappers/GlFrameBuffer.hpp"
#include "graphics/gl_wrappers/GlVertexArray.hpp"

namespace Engine {

class Renderer;

class IRenderContext
{
    friend class Renderer;

  protected:
    bool m_isRenderContextDirty = false;
    GlVertexArray* m_vertexArray = nullptr;
    GlFrameBuffer* m_contextBuffers[2] = {nullptr, nullptr};
    int m_currWriteIndex = 0;

  public:
    IRenderContext() = default;
    virtual ~IRenderContext() { delete m_vertexArray; }

  protected:
    virtual void bindRenderContext() = 0;
    virtual void unbindRenderContext() = 0;
    virtual int getRenderContextWidth() = 0;
    virtual int getRenderContextHeight() = 0;
    virtual float getRenderContextAspectRatio() = 0;

    virtual GlFrameBuffer* srcBuffer() { return m_contextBuffers[(m_currWriteIndex + 1) % 2]; }
    virtual GlFrameBuffer* destBuffer() { return m_contextBuffers[m_currWriteIndex]; }
    virtual void switchBuffers() { m_currWriteIndex = (m_currWriteIndex + 1) % 2; }
};

}   // namespace Engine
