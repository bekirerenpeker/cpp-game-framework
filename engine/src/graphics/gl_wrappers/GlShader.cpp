#include "graphics/gl_wrappers/GlShader.hpp"
#include "core/file_management/TextFile.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "graphics/Color.hpp"
#include "utils/math/Mat4.hpp"
#include "utils/math/Vec2.hpp"
#include "utils/math/Vec3.hpp"
#include "utils/math/Vec4.hpp"
#include "glad/glad.h"

namespace Engine {

Shader::Shader(const fs::path& sourceFileDir)
{
    ShaderSource source = parseSourceFile(sourceFileDir);
    m_GlId = createProgram(source);
    bind();
}

Shader::~Shader() { glDeleteShader(m_GlId); }

Shader::ShaderSource Shader::parseSourceFile(const fs::path& sourceFileDir)
{
    ShaderSource source = {"", ""};

    TextFile sourceFile(sourceFileDir);
    std::vector<std::string> lines = sourceFile.readLines();
    bool addingToVertex = true;

    for (const auto& line : lines) {
        if (line == "#shader vertex") addingToVertex = true;
        else if (line == "#shader frag") addingToVertex = false;
        else if (addingToVertex) source.vertexSource += line + "\n";
        else source.fragSource += line + "\n";
    }

    return source;
}

unsigned int Shader::createShader(const std::string& source, ShaderType type)
{
    unsigned int shaderID = glCreateShader((int)type);

    const char* sourceCStr = source.c_str();
    glShaderSource(shaderID, 1, &sourceCStr, NULL);
    glCompileShader(shaderID);

    int succes;
    char infoLog[512];
    glGetShaderiv(shaderID, GL_COMPILE_STATUS, &succes);
    if (!succes) {
        glGetShaderInfoLog(shaderID, 512, NULL, infoLog);
        LOG_ERROR(
            "couldn't compile {}\n{}",
            type == ShaderType::VertexShader ? "vertex shader:\n" : "fragment shader:\n", infoLog
        );
    }

    return shaderID;
}

unsigned int Shader::createProgram(const ShaderSource& source)
{
    unsigned int programID = glCreateProgram();

    glAttachShader(programID, createShader(source.vertexSource, ShaderType::VertexShader));
    glAttachShader(programID, createShader(source.fragSource, ShaderType::FragmentShader));
    glLinkProgram(programID);

    int succes;
    char infoLog[512];
    glGetProgramiv(programID, GL_LINK_STATUS, &succes);
    if (!succes) {
        glGetProgramInfoLog(programID, 512, NULL, infoLog);
        LOG_ERROR("couldn't link shader:\n{}", infoLog);
    }

    return programID;
}

void Shader::bind() const { glUseProgram(m_GlId); }

void Shader::unbind() const { glUseProgram(0); }

int Shader::getUniformLocation(const std::string& uniformName)
{
    if (m_UniformLocationCache.count(uniformName)) return m_UniformLocationCache[uniformName];
    int location = glGetUniformLocation(m_GlId, uniformName.c_str());

    if (location == -1) {
        LOG_WARNING("couldn't find uniform: {}", uniformName);
        return -1;
    }

    m_UniformLocationCache[uniformName] = location;
    return location;
}

template<> void Shader::setUniform<int>(const std::string& name, int val)
{
    bind();
    glUniform1i(getUniformLocation(name), val);
}
template<> void Shader::setUniform<float>(const std::string& name, float val)
{
    bind();
    glUniform1f(getUniformLocation(name), val);
}
template<> void Shader::setUniform<Vec2>(const std::string& name, Vec2 val)
{
    bind();
    glUniform2f(getUniformLocation(name), val.x, val.y);
}
template<> void Shader::setUniform<Vec3>(const std::string& name, Vec3 val)
{
    bind();
    glUniform3f(getUniformLocation(name), val.x, val.y, val.z);
}
template<> void Shader::setUniform<Vec4>(const std::string& name, Vec4 val)
{
    bind();
    glUniform4f(getUniformLocation(name), val.x, val.y, val.z, val.w);
}
template<> void Shader::setUniform<Color>(const std::string& name, Color val)
{
    bind();
    glUniform4f(getUniformLocation(name), val.r, val.g, val.b, val.a);
}
template<> void Shader::setUniform<Mat4>(const std::string& name, Mat4 val)
{
    bind();
    glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, &val[0][0]);
}

template<> void Shader::setUniformArr<int>(const std::string& name, int count, const int* arr)
{
    bind();
    glUniform1iv(getUniformLocation(name), count, arr);
}

}   // namespace Engine
