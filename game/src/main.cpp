#include "EngineInclude.hpp"
#include "ecs_test.hpp"

using namespace Engine;

int main()
{
    Logger::get().addSink<FileSink>("game/output/log.txt");
    ecs_test();
    return 0;
}
