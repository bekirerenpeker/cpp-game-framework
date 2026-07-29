#pragma once

#include "graphics/Color.hpp"
#include "graphics/text/TextStyle.hpp"
#include "graphics/ui/layout/ILeafMeasurer.hpp"
#include "graphics/ui/layout/LayoutTypes.hpp"
#include <cstdint>
#include <string>

namespace Engine {

class Font;
class GlTexture;

enum class UiElementType : uint8_t
{
    Container = 0,
    Text,
    Button,
    Image,
};

struct UiStyle
{
    Color backgroundColor = COLOR_CLEAR;
    Color borderColor = COLOR_CLEAR;
    Color contentColor = COLOR_WHITE;
    float borderWidth = 0.0f;
    float cornerRadius = 0.0f;
};

struct UiElement
{
    LayoutConfig layout;
    UiStyle style;
    TextStyle textStyle;

    std::string text;
    const Font* font = nullptr;
    const GlTexture* texture = nullptr;
    ILeafMeasurer* measurer = nullptr;

    uint parent = NO_NODE;
    UiElementType type = UiElementType::Container;
};

}   // namespace Engine
