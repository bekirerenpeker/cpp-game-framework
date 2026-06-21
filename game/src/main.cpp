#include "EngineInclude.hpp"
#include "ecs_test.hpp"
#include "file_management_test.hpp"
#include "piano_demo.hpp"

using namespace Engine;

int main()
{
    Logger::get().addSink<FileSink>("game/output/log.txt");
    piano_demo();
    return 0;
}
