#pragma once
#include "core/file_management/File.hpp"
#include <fstream>
#include <type_traits>

namespace Engine {

class BinaryFile : public File
{
  private:
    std::fstream m_stream;

  public:
    BinaryFile(const fs::path& path);
    ~BinaryFile() override;

    bool clear() override;

    // Stream Management
    bool open(std::ios::openmode mode = std::ios::in | std::ios::out | std::ios::binary);
    void close();
    bool isOpen() const;

    // Pointer Management (Seek)
    void seekRead(size_t position);
    void seekWrite(size_t position);
    size_t getReadPosition();
    size_t getWritePosition();

    // --- RAW BYTE OPERATIONS ---
    bool writeBytes(const uint8_t* data, size_t size);
    bool readBytes(uint8_t* buffer, size_t size);

    // --- SAFE STRUCT OPERATIONS (The Magic Feature) ---
    // This allows you to safely write POD structs directly!
    template<typename T> bool writeStruct(const T& data)
    {
        // This stops you from accidentally writing classes with pointers or virtuals!
        static_assert(
            std::is_standard_layout_v<T> && std::is_trivial_v<T>,
            "ERROR: You can only dump POD (Plain Old Data) structs to binary!"
        );

        return writeBytes(reinterpret_cast<const uint8_t*>(&data), sizeof(T));
    }

    template<typename T> bool readStruct(T& outData)
    {
        static_assert(
            std::is_standard_layout_v<T> && std::is_trivial_v<T>,
            "ERROR: You can only read POD (Plain Old Data) structs from binary!"
        );

        return readBytes(reinterpret_cast<uint8_t*>(&outData), sizeof(T));
    }
};

}   // namespace Engine
