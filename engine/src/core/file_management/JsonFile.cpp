#include "core/file_management/JsonFile.hpp"
#include <fstream>

namespace Engine {

// Note: Ensure you add FileType::JsonFile to your enum!
// For now, falling back to FileType::TextFile if it doesn't exist.
JsonFile::JsonFile(const fs::path& path) : File(path, FileType::TextFile)
{
    // Auto-load if the file already exists and is valid
    if (m_isValid && getSize() > 0) { load(); }
}

bool JsonFile::load()
{
    std::ifstream file(m_path);
    if (!file.is_open()) return false;

    try {
        // nlohmann::json overloads the >> operator to parse streams instantly
        file >> m_data;
        return true;
    } catch (const json::parse_error& e) {
        // Here you could use your logger: LOG_ERROR("JSON Parse Error: {}", e.what());
        m_data = json::object();   // Reset to an empty valid json object
        return false;
    }
}

bool JsonFile::save(int indent)
{
    // std::ios::trunc wipes the old file cleanly
    std::ofstream file(m_path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) return false;

    // std::setw formats the JSON beautifully with indentation
    file << std::setw(indent) << m_data << std::endl;
    return file.good();
}

JsonFile::json& JsonFile::getData() { return m_data; }
const JsonFile::json& JsonFile::getData() const { return m_data; }

bool JsonFile::clear()
{
    m_data.clear();            // Clears the JSON object in memory
    m_data = json::object();   // Sets it back to {}
    return File::clear();      // Truncates the file on disk to 0 bytes using your base class
}

}   // namespace Engine
