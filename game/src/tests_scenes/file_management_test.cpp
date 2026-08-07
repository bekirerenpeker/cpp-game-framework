#include "EngineInclude.hpp"
#include "test_funcs.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>

using namespace Engine;
namespace fs = std::filesystem;

// --- Dummy POD Struct for Binary Testing ---
struct PlayerSaveData
{
    uint32_t level;
    float health;
    float position[3];
};

int file_management_test()
{
    auto measure = [](const char* name, auto&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        LOG_INFO("[FILE] {} took {} ms", name, duration / 1000.0f);
    };

    LOG_INFO("============= FILE SYSTEM FEATURE & PERFORMANCE TESTS =============");

    fs::path outDir = fs::current_path() / "game" / "output";

    measure("Create Output Directory", [&]() {
        FileManager::get().createFolder(outDir);
        if (!FileManager::get().isDirectory(outDir))
            LOG_ERROR("Failed to create game/output folder!");
    });

    LOG_INFO("-----------------------------------------------------------");

    measure("TextFile: Write, Read, and Append", [&]() {
        fs::path textPath = outDir / "test_log.txt";
        FileManager::get().createFile(textPath);

        TextFile txt(textPath);
        txt.writeText("Line 1\nLine 2");
        txt.appendText("\nLine 3");

        std::vector<std::string> lines = txt.readLines();
        if (lines.size() != 3 || lines[2] != "Line 3") LOG_ERROR("TextFile operations failed!");
    });

    measure("BinaryFile: Write and Read POD Struct", [&]() {
        fs::path binPath = outDir / "player_save.bin";
        FileManager::get().createFile(binPath);

        PlayerSaveData outData = {
            42, 99.5f, {10.0f, 20.0f, 30.0f}
        };

        BinaryFile bin(binPath);
        bin.open(std::ios::out | std::ios::binary | std::ios::trunc);
        bin.writeStruct(outData);
        bin.close();

        PlayerSaveData inData = {
            0, 0.0f, {0.0f, 0.0f, 0.0f}
        };
        bin.open(std::ios::in | std::ios::binary);
        bin.readStruct(inData);
        bin.close();

        if (inData.level != 42 || inData.health != 99.5f || inData.position[2] != 30.0f)
            LOG_ERROR("Binary struct data corrupted or failed to read!");
    });

    measure("JsonFile: Write, Save, and Load values", [&]() {
        fs::path jsonPath = outDir / "config.json";
        FileManager::get().createFile(jsonPath);

        JsonFile jsonOut(jsonPath);
        jsonOut.setValue("ResolutionX", 1920);
        jsonOut.setValue("ResolutionY", 1080);
        jsonOut.setValue("Fullscreen", true);
        jsonOut.setValue("Title", std::string("My Engine"));
        jsonOut.save();

        JsonFile jsonIn(jsonPath);
        if (jsonIn.getValue<int>("ResolutionX", 0) != 1920 ||
            jsonIn.getValue<bool>("Fullscreen", false) != true) {
            LOG_ERROR("JSON load/save mismatch!");
        }
    });

    measure("ImageFile: Procedural Generation and Save", [&]() {
        fs::path imgPath = outDir / "texture.png";
        FileManager::get().createFile(imgPath);

        ImageData img(64, 64, 4);
        for (size_t y = 0; y < 64; ++y) {
            for (size_t x = 0; x < 64; ++x) {
                img.setValueAt(x, y, 0, 255);
                img.setValueAt(x, y, 1, 0);
                img.setValueAt(x, y, 2, 0);
                img.setValueAt(x, y, 3, 255);
            }
        }

        ImageFile imgFile(imgPath);
        bool saved = imgFile.saveImage(img);
        if (!saved || !FileManager::get().doesPathExist(imgPath))
            LOG_ERROR("Image file failed to save!");
    });

    LOG_INFO("-----------------------------------------------------------");

    measure("IFileEntry: Rename and Move operations", [&]() {
        fs::path subFolderPath = outDir / "subfolder";
        FileManager::get().createFolder(subFolderPath);

        File jsonToRename(outDir / "config.json");
        bool renamed = jsonToRename.rename("settings.json");

        File textToMove(outDir / "test_log.txt");
        bool moved = textToMove.moveTo(subFolderPath);

        if (!renamed || !moved || !FileManager::get().doesPathExist(outDir / "settings.json") ||
            !FileManager::get().doesPathExist(subFolderPath / "test_log.txt")) {
            LOG_ERROR("Rename or Move operations failed!");
        }
    });

    measure("Folder: Iterate Children", [&]() {
        Folder rootFolder(outDir);
        auto children = rootFolder.getChildren();

        size_t fileCount = 0;
        size_t folderCount = 0;

        for (const auto& child : children) {
            if (child->getType() == FileType::Folder) folderCount++;
            else fileCount++;
        }

        // Expects player_save.bin, settings.json, texture.png and one subfolder.
        if (fileCount != 3 || folderCount != 1) {
            LOG_ERROR("Folder child count mismatch! Files: {} Folders: {}", fileCount, folderCount);
        }
    });

    measure("IFileEntry: Delete From Disk", [&]() {
        fs::path tempPath = outDir / "to_delete.txt";
        FileManager::get().createFile(tempPath);

        File tempFile(tempPath);

#ifdef ALLOW_DELETE_FROM_DISK
        bool deleted = tempFile.deleteFromDisk();
        if (!deleted || FileManager::get().doesPathExist(tempPath) || tempFile.isValid())
            LOG_ERROR("Deletion failed!");
#else
        LOG_WARNING("Delete From Disk test skipped: ALLOW_DELETE_FROM_DISK is not defined.");
#endif
    });

    LOG_INFO("===========================================================\n");

    return 0;
}
