#pragma once

#include "Logger.hpp"

namespace Engine {

#ifdef DEBUG

#if OS_NAME == OS_WINDOWS
#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#else
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#define LOG_INFO(msg, ...)                                                                         \
    Logger::get().Log(                                                                             \
        LogLevel::Info, __FILENAME__, std::to_string(__LINE__), __FUNCTION__, __TIME__, msg,       \
        ##__VA_ARGS__                                                                              \
    )
#define LOG_WARNING(msg, ...)                                                                      \
    Logger::get().Log(                                                                             \
        LogLevel::Warning, __FILENAME__, std::to_string(__LINE__), __FUNCTION__, __TIME__, msg,    \
        ##__VA_ARGS__                                                                              \
    )
#define LOG_ERROR(msg, ...)                                                                        \
    Logger::get().Log(                                                                             \
        LogLevel::Error, __FILENAME__, std::to_string(__LINE__), __FUNCTION__, __TIME__, msg,      \
        ##__VA_ARGS__                                                                              \
    )

#else

#define LOG_INFO(msg, ...)
#define LOG_WARNING(msg, ...)
#define LOG_ERROR(msg, ...)

#endif

}   // namespace Engine
