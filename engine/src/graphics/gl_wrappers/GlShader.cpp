#include "graphics/gl_wrappers/GlShader.hpp"
#include "context/GladContext.hpp"
#include "core/file_management/TextFile.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "glad/glad.h"

namespace Engine {

GlShader::GlShader(const fs::path& sourceFileDir)
{
    GladContext::init();
    ShaderSource source = parseSourceFile(sourceFileDir);
    m_GlId = createProgram(source);
    bind();
}

GlShader::~GlShader() { glDeleteShader(m_GlId); }

GlShader::ShaderSource GlShader::parseSourceFile(const fs::path& sourceFileDir)
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

unsigned int GlShader::createShader(const std::string& source, ShaderType type)
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

unsigned int GlShader::createProgram(const ShaderSource& source)
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

void GlShader::bind() const { glUseProgram(m_GlId); }

void GlShader::unbind() const { glUseProgram(0); }

int GlShader::getUniformLocation(const std::string& uniformName)
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

}   // namespace Engine
