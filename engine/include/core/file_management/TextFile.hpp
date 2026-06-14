#pragma once

#include "core/file_management/File.hpp"
#include <string>
#include <vector>

namespace Engine {

class TextFile : public File
{
  public:
    TextFile(const fs::path& path);
    ~TextFile() override = default;

    std::string readText() const;
    std::vector<std::string> readLines() const;

    bool writeText(const std::string& text);
    bool writeLines(const std::vector<std::string>& lines, bool insertNewLine = true);

    bool appendText(const std::string& text);
    bool appendLines(const std::vector<std::string>& lines, bool insertNewLine = true);
};

}   // namespace Engine
