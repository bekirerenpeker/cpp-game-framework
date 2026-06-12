#include "logging/Debugger.hpp"
#include <iostream>

namespace Debugger {

std::string colorStr(FGColor fgColor) { return "\x1b[" + std::to_string((int)fgColor) + "m"; }
std::string colorStr(BGColor bgColor) { return "\x1b[" + std::to_string((int)bgColor) + "m"; }
std::string colorStr(FGColor fgColor, BGColor bgColor)
{
    return "\x1b[" + std::to_string((int)fgColor) + ";" + std::to_string((int)bgColor) + "m";
}
std::string defaultColorStr() { return "\x1b[39;49m"; }

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
