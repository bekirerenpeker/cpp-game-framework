#include "graphics/gl_wrappers/GlBuffer.hpp"
#include "glad/glad.h"
#include "utils/TypeAliases.hpp"

namespace Engine {

GlBuffer::GlBuffer(GlBufferType type)
{
    glGenBuffers(1, &m_GlId);
    bind();
}
GlBuffer::~GlBuffer() { glDeleteBuffers(1, &m_GlId); }

void GlBuffer::bind() const { glBindBuffer((uint)m_type, m_GlId); }
void GlBuffer::unbind() const { glBindBuffer((uint)m_type, 0); }

void GlBuffer::setData(long size, const void* data, GlBufferUsage usage)
{
    bind();
    glBufferData((uint)m_type, size, data, (uint)usage);
}

}   // namespace Engine
