#include "EngineInclude.hpp"
#include "piano_demo.hpp"

using namespace Engine;

int main()
{
    Logger::get().addSink<FileSink>("game/output/log.txt");
    return piano_demo();
}
