#include "EngineInclude.hpp"
#include "ecs_test.hpp"
#include "file_management_test.hpp"

using namespace Engine;

int main()
{
    Logger::get().addSink<FileSink>("game/output/log.txt");
    file_management_test();
    return 0;
}
