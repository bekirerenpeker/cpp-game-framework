#include "EngineInclude.hpp"
#include "test_funcs.hpp"

using namespace Engine;

int main()
{
    FileManager::get().setGameAssetRoot(GAME_ASSET_DIR);
    Logger::get().addSink<FileSink>("game/output/log.txt");
    return physics_test();
}
