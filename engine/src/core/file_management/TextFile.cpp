#include "core/file_management/TextFile.hpp"
#include <fstream>
#include <sstream>

namespace Engine {

TextFile::TextFile(const fs::path& path) : File(path, FileType::TextFile) {}

std::string TextFile::readText() const
{
    std::ifstream file(m_path);
    if (!file.is_open()) return "";

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::vector<std::string> TextFile::readLines() const
{
    std::vector<std::string> lines;
    std::ifstream file(m_path);
    if (!file.is_open()) return lines;

    std::string line;
    while (std::getline(file, line)) lines.push_back(line);

    return lines;
}

bool TextFile::writeText(const std::string& text)
{
    // std::ios::trunc means "truncate" - it wipes the file clean before writing
    std::ofstream file(m_path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) return false;

    file << text;
    return file.good();
}

bool TextFile::writeLines(const std::vector<std::string>& lines, bool insertNewLine)
{
    std::ofstream file(m_path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) return false;

    for (size_t i = 0; i < lines.size(); ++i) {
        file << lines[i];

        if (i < lines.size() - 1 && insertNewLine) file << '\n';
    }
    return file.good();
}

bool TextFile::appendText(const std::string& text)
{
    // std::ios::app ensures the write pointer starts at the very end of the file
    std::ofstream file(m_path, std::ios::out | std::ios::app);
    if (!file.is_open()) return false;

    file << text;
    return file.good();
}

bool TextFile::appendLines(const std::vector<std::string>& lines, bool insertNewLine)
{
    std::ofstream file(m_path, std::ios::out | std::ios::app);
    if (!file.is_open()) return false;

    for (const auto& line : lines) {
        file << line;
        if (insertNewLine) file << '\n';
    }
    return file.good();
}

}   // namespace Engine
