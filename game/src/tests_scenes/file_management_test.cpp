#pragma once

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

    // 1. Setup & Directory Creation
    measure("Create Output Directory", [&]() {
        FileManager::get().createFolder(outDir);
        if (FileManager::get().isDirectory(outDir)) {
            LOG_INFO("      SUCCESS: Folder game/output created/verified.");
        } else {
            LOG_ERROR("      ERROR: Failed to create game/output folder!");
        }
    });

    LOG_INFO("-----------------------------------------------------------");

    // 2. TextFile Test
    measure("TextFile: Write, Read, and Append", [&]() {
        fs::path textPath = outDir / "test_log.txt";
        FileManager::get().createFile(textPath);

        TextFile txt(textPath);
        txt.writeText("Line 1\nLine 2");
        txt.appendText("\nLine 3");

        std::vector<std::string> lines = txt.readLines();
        if (lines.size() == 3 && lines[2] == "Line 3") {
            LOG_INFO("      SUCCESS: Text written, appended, and read correctly.");
        } else {
            LOG_ERROR("      ERROR: TextFile operations failed!");
        }
    });

    // 3. BinaryFile Test
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

        if (inData.level == 42 && inData.health == 99.5f && inData.position[2] == 30.0f) {
            LOG_INFO("      SUCCESS: Binary POD struct matched expectations.");
        } else {
            LOG_ERROR("      ERROR: Binary struct data corrupted or failed to read!");
        }
    });

    // 4. JsonFile Test
    measure("JsonFile: Write, Save, and Load values", [&]() {
        fs::path jsonPath = outDir / "config.json";
        FileManager::get().createFile(jsonPath);

        JsonFile jsonOut(jsonPath);
        jsonOut.setValue("ResolutionX", 1920);
        jsonOut.setValue("ResolutionY", 1080);
        jsonOut.setValue("Fullscreen", true);
        jsonOut.setValue("Title", std::string("My Engine"));
        jsonOut.save();

        JsonFile jsonIn(jsonPath);   // Should auto-load in constructor based on your implementation
        if (jsonIn.getValue<int>("ResolutionX", 0) == 1920 &&
            jsonIn.getValue<bool>("Fullscreen", false) == true) {
            LOG_INFO("      SUCCESS: JSON File saved and loaded properties accurately.");
        } else {
            LOG_ERROR("      ERROR: JSON load/save mismatch!");
        }
    });

    // 5. ImageFile Test
    measure("ImageFile: Procedural Generation and Save", [&]() {
        fs::path imgPath = outDir / "texture.png";
        FileManager::get().createFile(imgPath);

        ImageData img(64, 64, 4);   // 64x64 RGBA
        for (size_t y = 0; y < 64; ++y) {
            for (size_t x = 0; x < 64; ++x) {
                img.setValueAt(x, y, 0, 255);   // R
                img.setValueAt(x, y, 1, 0);     // G
                img.setValueAt(x, y, 2, 0);     // B
                img.setValueAt(x, y, 3, 255);   // A
            }
        }

        ImageFile imgFile(imgPath);
        bool saved = imgFile.saveImage(img);

        if (saved && FileManager::get().doesPathExist(imgPath)) {
            LOG_INFO("      SUCCESS: Procedural PNG created and saved.");
        } else {
            LOG_ERROR("      ERROR: Image file failed to save!");
        }
    });

    LOG_INFO("-----------------------------------------------------------");

    // 6. Base File Operations Test (Rename, Move)
    measure("IFileEntry: Rename and Move operations", [&]() {
        fs::path subFolderPath = outDir / "subfolder";
        FileManager::get().createFolder(subFolderPath);

        // Rename the JSON file
        File jsonToRename(outDir / "config.json");
        bool renamed = jsonToRename.rename("settings.json");

        // Move the Text file
        File textToMove(outDir / "test_log.txt");
        bool moved = textToMove.moveTo(subFolderPath);

        if (renamed && moved && FileManager::get().doesPathExist(outDir / "settings.json") &&
            FileManager::get().doesPathExist(subFolderPath / "test_log.txt")) {
            LOG_INFO("      SUCCESS: Files successfully renamed and moved.");
        } else {
            LOG_ERROR("      ERROR: Rename or Move operations failed!");
        }
    });

    // 7. Folder Iteration Test
    measure("Folder: Iterate Children", [&]() {
        Folder rootFolder(outDir);
        auto children = rootFolder.getChildren();

        size_t fileCount = 0;
        size_t folderCount = 0;

        for (const auto& child : children) {
            if (child->getType() == FileType::Folder) folderCount++;
            else fileCount++;
        }

        // We expect 3 files in root (player_save.bin, settings.json, texture.png)
        // and 1 folder (subfolder)
        if (fileCount == 3 && folderCount == 1) {
            LOG_INFO("      SUCCESS: Folder child iteration detected expected layout.");
            LOG_INFO("      Total Size in Output: {} bytes.", rootFolder.getSize());
        } else {
            LOG_ERROR(
                "      ERROR: Folder child count mismatch! Files: {} Folders: {}", fileCount,
                folderCount
            );
        }
    });

    // 8. Deletion Test
    measure("IFileEntry: Delete From Disk", [&]() {
        fs::path tempPath = outDir / "to_delete.txt";
        FileManager::get().createFile(tempPath);

        File tempFile(tempPath);

#ifdef ALLOW_DELETE_FROM_DISK
        bool deleted = tempFile.deleteFromDisk();
        if (deleted && !FileManager::get().doesPathExist(tempPath) && !tempFile.isValid()) {
            LOG_INFO("      SUCCESS: File properly deleted from disk and invalidated.");
        } else {
            LOG_ERROR("      ERROR: Deletion failed!");
        }
#else
        LOG_WARNING("      SKIPPED: ALLOW_DELETE_FROM_DISK is not defined.");
#endif
    });

    LOG_INFO("===========================================================\n");

    return 0;
}
