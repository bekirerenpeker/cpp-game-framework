#include "EngineInclude.hpp"

using namespace Engine;

int main()
{
    Logger::get().Log(
        LogLevel::Info, __FILE__, std::to_string(__LINE__), __FUNCTION__, "00:00:00",
        "Hello, World!"
    );

    return 0;
}
