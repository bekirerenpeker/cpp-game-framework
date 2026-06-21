#pragma once

#include "core/file_management/File.hpp"
#include "nlohmann/json.hpp"

namespace Engine {

class JsonFile : public File
{
  public:
    // Alias for convenience
    using json = nlohmann::json;

  private:
    json m_data;

  public:
    // NOTE: You will need to add `JsonFile` to your FileType enum in IFileEntry.hpp
    JsonFile(const fs::path& path);
    ~JsonFile() override = default;

    // Disk operations
    bool load();
    bool save(int indent = 4);

    // Direct access to the underlying json object if needed
    json& getData();
    const json& getData() const;

    // --- TYPE-SAFE HELPERS ---
    // These completely solve the "dynamic type disaster" worry.

    template<typename T> T getValue(const std::string& key, const T& defaultValue = T {}) const
    {
        if (m_data.contains(key) && !m_data[key].is_null()) {
            try {
                // Safely attempts to cast the json node to your requested C++ type
                return m_data[key].get<T>();
            } catch (const json::type_error&) {
                // If the user put a string in the JSON but you asked for an int,
                // it safely catches it and returns the default value.
                return defaultValue;
            }
        }
        return defaultValue;
    }

    template<typename T> void setValue(const std::string& key, const T& value)
    {
        m_data[key] = value;
    }

    // Clear the JSON data and the file
    bool clear() override;
};

}   // namespace Engine
