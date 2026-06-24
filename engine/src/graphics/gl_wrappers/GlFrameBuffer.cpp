#include "graphics/gl_wrappers/GlFrameBuffer.hpp"
#include "glad/glad.h"
#include "graphics/gl_wrappers/GlTexture.hpp"

namespace Engine {

GlFrameBuffer::GlFrameBuffer(int width, int height) : m_width(width), m_height(height)
{
    glGenFramebuffers(1, &m_GlId);
    glBindFramebuffer(GL_FRAMEBUFFER, m_GlId);

    glGenTextures(1, &m_colorAttachment);
    glBindTexture(GL_TEXTURE_2D, m_colorAttachment);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorAttachment, 0
    );

    // Wrap the ID in your existing system
    m_texture = new GlTexture(m_colorAttachment, width, height, 4);

    // Depth/Stencil buffer setup
    glGenRenderbuffers(1, &m_depthAttachment);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthAttachment);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(
        GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthAttachment
    );

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GlFrameBuffer::~GlFrameBuffer()
{
    glDeleteFramebuffers(1, &m_GlId);
    delete m_texture;
    glDeleteRenderbuffers(1, &m_depthAttachment);
}

void GlFrameBuffer::bind() const { glBindFramebuffer(GL_FRAMEBUFFER, m_GlId); }
void GlFrameBuffer::unbind() const { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

GlTexture* GlFrameBuffer::getTexture() const { return m_texture; }

}   // namespace Engine
