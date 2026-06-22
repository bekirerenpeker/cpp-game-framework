#include "context/GlfwContext.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "GLFW/glfw3.h"

namespace Engine {

namespace GlfwContext {

static bool initialized = false;

static void glfwDebugOutput(int error, const char* description);

void init()
{
    if (initialized) return;

    if (glfwInit() == GLFW_FALSE) {
        LOG_ERROR("couldn't initialize glfw");
        return;
    }
    glfwSetErrorCallback(glfwDebugOutput);

    initialized = true;
}

void quit()
{
    if (!initialized) return;
    glfwTerminate();
    initialized = false;
}

void pollEvents()
{
    if (initialized) glfwPollEvents();
}

static void glfwDebugOutput(int error, const char* description)
{
    LOG_ERROR("[GLFW]: {}, {}", error, description);
}

};   // namespace GlfwContext

}   // namespace Engine
