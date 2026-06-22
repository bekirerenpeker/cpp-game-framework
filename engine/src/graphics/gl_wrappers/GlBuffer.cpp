#include "graphics/gl_wrappers/GlBuffer.hpp"
#include "glad/glad.h"
#include "utils/TypeAliases.hpp"

namespace Engine {

GlBuffer::GlBuffer(GlBufferType type) : m_type(type)
{
    glGenBuffers(1, &m_GlId);
    bind();
}
GlBuffer::~GlBuffer() { glDeleteBuffers(1, &m_GlId); }

void GlBuffer::bind() const { glBindBuffer(glBufferTypeVal(m_type), m_GlId); }
void GlBuffer::unbind() const { glBindBuffer(glBufferTypeVal(m_type), 0); }

void GlBuffer::setData(long size, const void* data, GlBufferUsage usage)
{
    bind();
    glBufferData(glBufferTypeVal(m_type), size, data, glBufferUsageVal(usage));
}

unsigned int GlBuffer::glBufferTypeVal(GlBufferType type) const
{
    switch (type) {
    case GlBufferType::VertexBuffer: return GL_ARRAY_BUFFER;
    case GlBufferType::IndexBuffer : return GL_ELEMENT_ARRAY_BUFFER;
    }
}
unsigned int GlBuffer::glBufferUsageVal(GlBufferUsage usage) const
{
    switch (usage) {
    case GlBufferUsage::StaticDraw : return GL_STATIC_DRAW;
    case GlBufferUsage::DynamicDraw: return GL_DYNAMIC_DRAW;
    }
}

}   // namespace Engine
