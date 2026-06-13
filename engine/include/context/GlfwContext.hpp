#pragma once

#include "core/logging/LoggerMacros.hpp"
#include "GLFW/glfw3.h"

namespace Engine {

namespace GlfwContext {

static bool initialized = false;

void init()
{
    if (initialized) return;

    if (glfwInit() == GLFW_FALSE) {
        LOG_ERROR("couldn't initialize glfw");
        return;
    }
    glfwSetErrorCallback(Logger::logGlfwMessage);

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

};   // namespace GlfwContext

}   // namespace Engine
