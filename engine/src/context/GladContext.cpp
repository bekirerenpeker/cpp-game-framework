#include "context/GladContext.hpp"
#include "core/logging/LoggerMacros.hpp"

// glad must be included before glfw
#include "glad/glad.h"
#include "GLFW/glfw3.h"

namespace Engine {

namespace GladContext {

static bool initialized = false;

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
}

void quit()
{
    if (!initialized) return;

    // Unlike GLFW, GLAD does not allocate massive underlying OS resources that require
    // an explicit terminate function. Resetting the flag maintains state consistency.
    initialized = false;
}

}   // namespace GladContext

}   // namespace Engine
