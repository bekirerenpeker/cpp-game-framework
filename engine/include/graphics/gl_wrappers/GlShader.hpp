#pragma once

#include <string>
#include <unordered_map>
#include "core/logging/LoggerMacros.hpp"
#include "graphics/gl_wrappers/IGlObject.hpp"
#include "core/resource_management/IResource.hpp"
#include "core/file_management/FileManager.hpp"
#include "utils/math/Vec2.hpp"
#include "utils/math/Vec3.hpp"
#include "utils/math/Vec4.hpp"
#include "utils/math/Mat4.hpp"
#include "graphics/Color.hpp"
#include <glad/glad.h>

namespace Engine {

class GlShader : public IGlObject, public IResource
{
  private:
    struct ShaderSource
    {
        std::string vertexSource, fragSource;
    };
    enum class ShaderType
    {
        VertexShader = 0x8B31,
        FragmentShader = 0x8B30,
    };

    std::unordered_map<std::string, int> m_UniformLocationCache;

  public:
    GlShader(const fs::path& sourceFileDir);
    ~GlShader();

    GlShader(GlShader&&) = default;
    GlShader& operator=(GlShader&&) = default;

    void bind() const override;
    void unbind() const override;

    // Generic templates remain as warnings; specializations are defined inline below.
    template<typename T> void setUniform(const std::string& name, T val)
    {
        LOG_WARNING("this type has not been implemented yet");
    }

    template<typename T> void setUniformArr(const std::string& name, int count, const T* arr)
    {
        LOG_WARNING("this type of array has not been implemented yet");
    }

  private:
    ShaderSource parseSourceFile(const fs::path& sourceFileDir);
    unsigned int createShader(const std::string& source, ShaderType type);
    unsigned int createProgram(const ShaderSource& source);

    int getUniformLocation(const std::string& uniformName);
};

// Inline specializations for supported types
template<> inline void GlShader::setUniform<int>(const std::string& name, int val)
{
    bind();
    glUniform1i(getUniformLocation(name), val);
}
template<> inline void GlShader::setUniform<float>(const std::string& name, float val)
{
    bind();
    glUniform1f(getUniformLocation(name), val);
}
template<> inline void GlShader::setUniform<Vec2>(const std::string& name, Vec2 val)
{
    bind();
    glUniform2f(getUniformLocation(name), val.x, val.y);
}
template<> inline void GlShader::setUniform<Vec3>(const std::string& name, Vec3 val)
{
    bind();
    glUniform3f(getUniformLocation(name), val.x, val.y, val.z);
}
template<> inline void GlShader::setUniform<Vec4>(const std::string& name, Vec4 val)
{
    bind();
    glUniform4f(getUniformLocation(name), val.x, val.y, val.z, val.w);
}
template<> inline void GlShader::setUniform<Color>(const std::string& name, Color val)
{
    bind();
    glUniform4f(getUniformLocation(name), val.r, val.g, val.b, val.a);
}
template<> inline void GlShader::setUniform<Mat4>(const std::string& name, Mat4 val)
{
    bind();
    glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, &val[0][0]);
}

// Array specialization for int
template<>
inline void GlShader::setUniformArr<int>(const std::string& name, int count, const int* arr)
{
    bind();
    glUniform1iv(getUniformLocation(name), count, arr);
}

}   // namespace Engine
