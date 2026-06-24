#pragma once

#include "graphics/gl_wrappers/GlTexture.hpp"
#include "graphics/gl_wrappers/IGlObject.hpp"

namespace Engine {

class GlFrameBuffer : public IGlObject
{
  private:
    unsigned int m_colorAttachment = 0;
    unsigned int m_depthAttachment = 0;
    int m_width, m_height;
    GlTexture* m_texture = nullptr;

  public:
    GlFrameBuffer(int width, int height);
    ~GlFrameBuffer() override;

    void bind() const override;
    void unbind() const override;

    GlTexture* getTexture() const;
};

}   // namespace Engine
