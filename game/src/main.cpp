#include "EngineInclude.hpp"

using namespace Engine;

int main()
{
    Logger::get().addSink<FileSink>("game/output/log.txt");

    return 0;
}
