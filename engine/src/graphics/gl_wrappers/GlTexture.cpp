#include "graphics/gl_wrappers/GlTexture.hpp"
#include "context/GladContext.hpp"
#include "core/file_management/ImageFile.hpp"
#include "glad/glad.h"
#include <iostream>

namespace Engine {

GlTexture::GlTexture(
    const fs::path& imgPath, int desiredChannels, FilterMode filterMode, WrapMode wrapMode
)
{
    ImageFile imgFile(imgPath);
    imgFile.loadImage(desiredChannels);
    m_GlId = createTexture(imgFile.getImageData(), filterMode, wrapMode);
    bind();
}

GlTexture::GlTexture(const ImageData& imageData, FilterMode filterMode, WrapMode wrapMode)
{
    m_GlId = createTexture(imageData, filterMode, wrapMode);
    bind();
}

GlTexture::GlTexture(unsigned int glId, int width, int height, int depth)
    : m_width(width), m_height(height), m_depth(depth)
{
    m_GlId = glId;
}

GlTexture::GlTexture(Color color)
{
    ImageData imageData(1, 1, 4, new unsigned char[4]);
    imageData.pixels[0] = (unsigned int)(color.r * 255);
    imageData.pixels[1] = (unsigned int)(color.g * 255);
    imageData.pixels[2] = (unsigned int)(color.b * 255);
    imageData.pixels[3] = (unsigned int)(color.a * 255);

    m_GlId = createTexture(imageData, FilterMode::Point, WrapMode::Repeat);

    bind();
}

unsigned int
GlTexture::createTexture(const ImageData& imageData, FilterMode filterMode, WrapMode wrapMode)
{
    GladContext::init();

    m_width = imageData.width;
    m_height = imageData.height;
    m_depth = imageData.depth;

    glGenTextures(1, &m_GlId);
    glBindTexture(GL_TEXTURE_2D, m_GlId);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (int)wrapMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (int)wrapMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (int)filterMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (int)filterMode);

    unsigned int format, internalFormat;
    switch (imageData.depth) {
    case 1:
        format = GL_RED;
        internalFormat = GL_RED;
        break;
    case 2:
        format = GL_RG;
        internalFormat = GL_RG;
        break;
    case 3:
        format = GL_RGB;
        internalFormat = GL_RGB;
        break;
    case 4:
        format = GL_RGBA;
        internalFormat = GL_RGBA;
        break;
    }

    glTexImage2D(
        GL_TEXTURE_2D, 0, internalFormat, imageData.width, imageData.height, 0, format,
        GL_UNSIGNED_BYTE, imageData.pixels
    );

    return m_GlId;
}

void GlTexture::bind(int slot) const
{
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, m_GlId);
}

void GlTexture::unbind() const { glBindTexture(GL_TEXTURE_2D, 0); }

}   // namespace Engine
