#pragma once

#include "graphics/gl_wrappers/GlFrameBuffer.hpp"
#include "graphics/gl_wrappers/GlVertexArray.hpp"
#include <unordered_map>

namespace Engine {

class Renderer;

class IRenderContext
{
    friend class Renderer;

  protected:
    bool m_isRenderContextDirty = false;
    // One VAO per owner (Renderer, TilemapRenderer, ...). VAOs are not shared
    // across GL contexts the way buffers/textures are, so every context keeps
    // its own set, torn down with the context.
    std::unordered_map<const void*, GlVertexArray*> m_vertexArrays;
    GlFrameBuffer* m_contextBuffers[2] = {nullptr, nullptr};
    int m_currWriteIndex = 0;

  public:
    IRenderContext() = default;
    virtual ~IRenderContext()
    {
        for (auto& [owner, vao] : m_vertexArrays) delete vao;
    }

    // This context's VAO slot for the given owner, created (as nullptr) on first
    // access. The owner fills it in and it is destroyed with the context.
    GlVertexArray*& vertexArray(const void* owner) { return m_vertexArrays[owner]; }

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
