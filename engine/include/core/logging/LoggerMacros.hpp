#pragma once

#include "Logger.hpp"
#include <format>

namespace Engine {

#ifdef DEBUG

#if OS_NAME == OS_WINDOWS
#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#else
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#define LOG_INFO(msg, ...)                                                                         \
    Logger::get().Log(                                                                             \
        LogLevel::Info, __FILENAME__, std::to_string(__LINE__), __FUNCTION__, msg, ##__VA_ARGS__   \
    )
#define LOG_WARNING(msg, ...)                                                                      \
    Logger::get().Log(                                                                             \
        LogLevel::Warning, __FILENAME__, std::to_string(__LINE__), __FUNCTION__, msg,              \
        ##__VA_ARGS__                                                                              \
    )
#define LOG_ERROR(msg, ...)                                                                        \
    Logger::get().Log(                                                                             \
        LogLevel::Error, __FILENAME__, std::to_string(__LINE__), __FUNCTION__, msg, ##__VA_ARGS__  \
    )

#else

#define LOG_INFO(msg, ...)
#define LOG_WARNING(msg, ...)
#define LOG_ERROR(msg, ...)

#endif

// The Master Macro
#define DEFINE_TYPE_FORMATTER(Type, FormatString, ...)                                             \
    template<> struct std::formatter<Type>                                                         \
    {                                                                                              \
        constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }               \
        auto format(const Type& obj, std::format_context& ctx) const                               \
        {                                                                                          \
            return std::format_to(ctx.out(), FormatString, __VA_ARGS__);                           \
        }                                                                                          \
    };

}   // namespace Engine
