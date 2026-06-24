#include "graphics/gl_wrappers/GlVertexArray.hpp"
#include "context/GladContext.hpp"
#include "glad/glad.h"
#include "graphics/gl_wrappers/GlLayout.hpp"

namespace Engine {

GlVertexArray::GlVertexArray()
{
    GladContext::init();
    glGenVertexArrays(1, &m_GlId);
    bind();
}

GlVertexArray::~GlVertexArray() { glDeleteVertexArrays(1, &m_GlId); }

void GlVertexArray::bind() const { glBindVertexArray(m_GlId); }
void GlVertexArray::unbind() const { glBindVertexArray(0); }

void GlVertexArray::setLayout(const GlLayout& layout)
{
    bind();

    unsigned int index = 0;
    long long offset = 0;
    int stride = 0;

    for (const auto& element : layout) stride += dataTypeSize(element.dataType) * element.count;

    for (const auto& element : layout) {
        glEnableVertexAttribArray(index);

        if ((int)element.dataType < (int)GlDataType::Float) {
            glVertexAttribIPointer(
                index, element.count, glDataTypeVal(element.dataType), stride, (const void*)offset
            );
        } else {
            glVertexAttribPointer(
                index, element.count, glDataTypeVal(element.dataType), GL_FALSE, stride,
                (const void*)offset
            );
        }

        offset += dataTypeSize(element.dataType) * element.count;
        index++;
    }
}

unsigned int GlVertexArray::dataTypeSize(GlDataType dataType) const
{
    switch (dataType) {
    case GlDataType::Byte         : return 1;
    case GlDataType::UnsignedByte : return 1;
    case GlDataType::Short        : return 2;
    case GlDataType::UnsignedShort: return 2;
    case GlDataType::Int          : return 4;
    case GlDataType::UnsignedInt  : return 4;
    case GlDataType::Float        : return 4;
    case GlDataType::Double       : return 4;
    case GlDataType::HalfFloat    : return 2;
    case GlDataType::Fixed        : return 4;
    }
}

unsigned int GlVertexArray::glDataTypeVal(GlDataType dataType) const
{
    switch (dataType) {
    case GlDataType::Byte         : return GL_BYTE;
    case GlDataType::UnsignedByte : return GL_UNSIGNED_BYTE;
    case GlDataType::Short        : return GL_SHORT;
    case GlDataType::UnsignedShort: return GL_UNSIGNED_SHORT;
    case GlDataType::Int          : return GL_INT;
    case GlDataType::UnsignedInt  : return GL_UNSIGNED_INT;
    case GlDataType::Float        : return GL_FLOAT;
    case GlDataType::Double       : return GL_DOUBLE;
    case GlDataType::HalfFloat    : return GL_HALF_FLOAT;
    case GlDataType::Fixed        : return GL_FIXED;
    }
}

}   // namespace Engine
