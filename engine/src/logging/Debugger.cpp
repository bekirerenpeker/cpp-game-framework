#include "logging/Debugger.hpp"
#include <iostream>

namespace Debugger {

static std::string colorStr(FGColor fgColor) { return "\e[" + std::to_string((int)fgColor) + "m"; }
static std::string colorStr(BGColor bgColor) { return "\e[" + std::to_string((int)bgColor) + "m"; }
static std::string colorStr(FGColor fgColor, BGColor bgColor)
{
    return "\e[" + std::to_string((int)fgColor) + ";" + std::to_string((int)bgColor) + "m";
}
static std::string defaultColorStr() { return "\e[39;49m"; }

void glfwMessageCallback(int error, const char* msg)
{
    std::cout << colorStr(FGColor::BrightRed) << "[GLFW_ERROR] " << defaultColorStr() << msg
              << "\n";
}

void glewMessageCallback(
    uint source, uint type, uint id, uint severity, int length, const char* msg,
    const void* userParam
)
{
    std::cout << colorStr(FGColor::BrightRed) << "[GLEW_ERROR] " << defaultColorStr() << msg
              << "\n";
}

}   // namespace Debugger
