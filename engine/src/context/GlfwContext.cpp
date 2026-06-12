#include "context/GlfwContext.hpp"
#include "logging/Debugger.hpp"
#include <GLFW/glfw3.h>
#include <iostream>

namespace GlfwContext {

static bool s_IsInitialized = false;

bool Init()
{
    if (s_IsInitialized) return true;

    if (!glfwInit()) {
        LOG_ERROR("[GLFW] Failed to initialize!");
        return false;
    }

    s_IsInitialized = true;
    return true;
}

void Quit()
{
    if (!s_IsInitialized) return;

    glfwTerminate();
    s_IsInitialized = false;
    LOG_INFO("[GLFW] Terminated successfully.");
}

bool IsInitialized() { return s_IsInitialized; }

}   // namespace GlfwContext
