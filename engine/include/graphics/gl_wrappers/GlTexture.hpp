#pragma once

#include "IGlObject.hpp"
#include "core/resource_management/IResource.hpp"
#include "graphics/Color.hpp"
#include "utils/ImageData.hpp"
#include "utils/math/Vec2.hpp"
#include "core/file_management/FileManager.hpp"

namespace Engine {

class GlTexture : public IGlObject, public IResource
{
  private:
    int m_width, m_height, m_depth;

  public:
    enum class WrapMode
    {
        Repeat = 0x2901,
        MirroredRepeat = 0x8370,
        ClampToEdge = 0x812F,
        ClampToBorder = 0x812D,
    };
    enum class FilterMode
    {
        Point = 0x2600,
        Linear = 0x2601,
    };

  public:
    GlTexture(
        const fs::path& imgPath, int desiredChannels = 0, FilterMode filterMode = FilterMode::Point,
        WrapMode wrapMode = WrapMode::Repeat
    );
    GlTexture(
        const ImageData& imageData, FilterMode filterMode = FilterMode::Point,
        WrapMode wrapMode = WrapMode::Repeat
    );
    GlTexture(unsigned int glId, int width, int height, int depth);
    GlTexture(Color color);

    GlTexture(GlTexture&&) = default;
    GlTexture& operator=(GlTexture&&) = default;

    void bind() const override { bind(0); }
    void bind(int slot) const;
    void unbind() const override;

    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    int getChannels() const { return m_depth; }
    Vec2 getSize() const { return Vec2(m_width, m_height); }

  private:
    unsigned int
    createTexture(const ImageData& imageData, FilterMode filterMode, WrapMode wrapMode);
};

}   // namespace Engine
