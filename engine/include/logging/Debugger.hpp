#pragma once

#include "logging/DebuggerColors.hpp"
#include <string>
#include <iostream>

// #ifdef Windows
// #define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
// #else
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
// #endif

namespace Debugger {

std::string colorStr(FGColor fgColor);
std::string colorStr(BGColor bgColor);
std::string colorStr(FGColor fgColor, BGColor bgColor);
std::string defaultColorStr();

typedef unsigned int uint;
void glfwMessageCallback(int error, const char* msg);
void glewMessageCallback(
    uint source, uint type, uint id, uint severity, int length, const char* msg,
    const void* userParam
);

};   // namespace Debugger

#define SPACING(count) std::cout << std::string("\n", count)

#define LOG_INFO(msg)                                                                              \
    std::cout << Debugger::colorStr(Debugger::FGColor::Cyan) << "[INFO : " << __FILENAME__         \
              << " : " << __FUNCTION__ << "() : " << __LINE__ << "] "                              \
              << Debugger::defaultColorStr() << msg << "\n";

#define LOG_WARNING(msg)                                                                           \
    std::cout << Debugger::colorStr(Debugger::FGColor::Yellow) << "[WARNING : " << __FILENAME__    \
              << " : " << __FUNCTION__ << "() : " << __LINE__ << "] "                              \
              << Debugger::defaultColorStr() << msg << "\n";

#define LOG_ERROR(msg)                                                                             \
    std::cout << Debugger::colorStr(Debugger::FGColor::BrightRed) << "[ERROR : " << __FILENAME__   \
              << " : " << __FUNCTION__ << "() : " << __LINE__ << "] "                              \
              << Debugger::defaultColorStr() << msg << "\n";
