#pragma once

#include "Logger.hpp"

namespace Engine {

#ifdef DEBUG

#define LOG_INFO(msg, ...)                                                                         \
    Logger::get().Log(                                                                             \
        LogLevel::Info, __FILE__, std::to_string(__LINE__), __FUNCTION__, __TIME__, msg,           \
        ##__VA_ARGS__                                                                              \
    )
#define LOG_WARNING(msg, ...)                                                                      \
    Logger::get().Log(                                                                             \
        LogLevel::Warning, __FILE__, std::to_string(__LINE__), __FUNCTION__, __TIME__, msg,        \
        ##__VA_ARGS__                                                                              \
    )
#define LOG_ERROR(msg, ...)                                                                        \
    Logger::get().Log(                                                                             \
        LogLevel::Error, __FILE__, std::to_string(__LINE__), __FUNCTION__, __TIME__, msg,          \
        ##__VA_ARGS__                                                                              \
    )

#else

#define LOG_INFO(msg, ...)
#define LOG_WARNING(msg, ...)
#define LOG_ERROR(msg, ...)

#endif

}   // namespace Engine
