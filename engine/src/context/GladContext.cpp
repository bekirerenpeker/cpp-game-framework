#include "context/GladContext.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "GLFW/glfw3.h"
#include "glad/glad.h"

namespace Engine {

namespace GladContext {

static bool initialized = false;

static void glDebugOutput(
    GLenum source, GLenum type, unsigned int id, GLenum severity, GLsizei length,
    const char* message, const void* userParam
);

void init()
{
    if (initialized) return;

    // gladLoadGLLoader requires the OS-specific function pointer loader from GLFW
    // It returns 0 (false) if it fails to load the context.
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        LOG_ERROR("couldn't initialize GLAD");
        return;
    }

    initialized = true;

    applyContextOptions();
}

void quit()
{
    if (!initialized) return;

    // Unlike GLFW, GLAD does not allocate massive underlying OS resources that require
    // an explicit terminate function. Resetting the flag maintains state consistency.
    initialized = false;
}

void applyContextOptions()
{
    if (!initialized) return;

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);   // Makes it easier to set breakpoints
    glDebugMessageCallback(glDebugOutput, nullptr);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDisable(GL_DEPTH_TEST);
}

static void glDebugOutput(
    GLenum source, GLenum type, unsigned int id, GLenum severity, GLsizei length,
    const char* message, const void* userParam
)
{
    // Ignore non-significant error codes
    // if (id == 131169 || id == 131185 || id == 131218 || id == 131204) return;

    std::string msg = "[OpenGL] " + std::string(message);

    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH        : LOG_ERROR(msg); break;
    case GL_DEBUG_SEVERITY_MEDIUM      : LOG_WARNING(msg); break;
    case GL_DEBUG_SEVERITY_LOW         : LOG_INFO(msg); break;
    case GL_DEBUG_SEVERITY_NOTIFICATION: LOG_INFO(msg); break;
    }
}

}   // namespace GladContext

}   // namespace Engine
