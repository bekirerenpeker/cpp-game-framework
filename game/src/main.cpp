#include "EngineInclude.hpp"
#include "test_funcs.hpp"

using namespace Engine;

int main()
{
    Logger::get().addSink<FileSink>("game/output/log.txt");
    return batch_renderer_test();
}
