#pragma once

#include "utils/Singleton.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Engine {

// `neighbors` is an 8-char string, from N (top) clockwise (N,NE,E,SE,S,SW,W,NW):
// '1' = must connect, '0' = must not connect, any other char = don't care.
struct RuleTileRule
{
    std::string neighbors;   // 8 chars N-clockwise, e.g. "1x1xxxxx": '1' connect, '0' not, else any
    uint8_t regionIndex = 0;
    int rotation = 0;
};

struct RuleTileMapping
{
    uint8_t regionIndex = 0;
    int rotation = 0;
};

class RuleTileTemplateManager : public Singleton<RuleTileTemplateManager>
{
    friend class Singleton<RuleTileTemplateManager>;

  private:
    std::unordered_map<std::string, std::array<RuleTileMapping, 256>> m_compiled;

  public:
    void createTemplate(const std::string& name, const std::vector<RuleTileRule>& rules);
    const std::array<RuleTileMapping, 256>& getTemplate(const std::string& name);
    int templateRegionCount(const std::string& name);

  private:
    RuleTileTemplateManager() = default;
    ~RuleTileTemplateManager() = default;
    static std::array<RuleTileMapping, 256> compile(const std::vector<RuleTileRule>& rules);
    static std::vector<RuleTileRule> builtin16Tile();
    static std::vector<RuleTileRule> builtin47Tile();
};

}   // namespace Engine
