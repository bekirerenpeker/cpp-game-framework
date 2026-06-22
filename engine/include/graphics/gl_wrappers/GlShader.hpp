#pragma once

#include <string>
#include <unordered_map>
#include "core/logging/LoggerMacros.hpp"
#include "graphics/gl_wrappers/IGlObject.hpp"
#include "core/resource_management/IResource.hpp"
#include "core/file_management/FileManager.hpp"

namespace Engine {

class Shader : public IGlObject, public IResource
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
    Shader(const fs::path& sourceFileDir);
    ~Shader();

    void bind() const override;
    void unbind() const override;

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

}   // namespace Engine
