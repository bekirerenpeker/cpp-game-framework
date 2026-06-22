#pragma once

namespace Engine {

class Vec4;

class Color
{
  public:
    float r, g, b, a;

  public:
    Color(float r, float g, float b, float a);
    Color(float c, float a = 1);
    Color();

    operator Vec4() const;
};

#define COLOR_BLACK   Color(0, 0, 0, 1)
#define COLOR_BLUE    Color(0, 0, 1, 1)
#define COLOR_CLEAR   Color(0, 0, 0, 0)
#define COLOR_CYAN    Color(0, 1, 1, 1)
#define COLOR_GREEN   Color(0, 1, 0, 1)
#define COLOR_MAGENTA Color(1, 0, 1, 1)
#define COLOR_RED     Color(1, 0, 0, 1)
#define COLOR_WHITE   Color(1, 1, 1, 1)
#define COLOR_YELLOW  Color(1, .92f, .016f, 1)   // normally it is Color(1, 1, 0, 1)
#define COLOR_GRAY    Color(.5f, .5f, .5f, 1)
#define COLOR_GREY    COLOR_GRAY

}   // namespace Engine
