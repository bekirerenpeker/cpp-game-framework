#pragma once

#include "core/input/KeyCodes.hpp"
#include "core/input/InputAxis.hpp"
#include "utils/Singleton.hpp"
#include "utils/TypeAliases.hpp"
#include "utils/math/Vec2.hpp"
#include <string>
#include <unordered_map>

namespace Engine {

class Input : public Singleton<Input>
{
    friend class Singleton<Input>;

  private:
    static constexpr unsigned int KEY_COUNT = (uint)KeyCode::KeyCount;
    static constexpr unsigned int BUTTON_COUNT = (uint)MouseButton::ButtonCount;
    static constexpr unsigned int glfwKeyCodes[KEY_COUNT] = {
        32,  39,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  59,  61,
        65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,  81,  82,
        83,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  96,  161, 162, 256, 257, 258, 259,
        260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 280, 281, 282, 283, 284, 290, 291, 292,
        293, 294, 295, 296, 297, 298, 299, 300, 301, 302, 303, 304, 305, 306, 307, 308, 309, 310,
        311, 312, 313, 314, 320, 321, 322, 323, 324, 325, 326, 327, 328, 329, 330, 331, 332, 333,
        334, 335, 336, 340, 341, 342, 343, 344, 345, 346, 347, 348,
    };

    bool m_keyCurrState[KEY_COUNT], m_keyPrevState[KEY_COUNT];
    bool m_buttonCurrState[BUTTON_COUNT], m_buttonPrevState[BUTTON_COUNT];
    Vec2 m_mousePos;

    std::unordered_map<std::string, InputAxis> m_axises;

  public:
    void update(IdType windowId = INVALID_ID);

    void addAxis(const std::string& name, const InputAxis& axis);
    int getAxis(const std::string& name);

    bool keyPressed(KeyCode key);
    bool keyReleased(KeyCode key);
    bool keyHeld(KeyCode key);

    bool mouseButtonPressed(MouseButton button);
    bool mouseButtonReleased(MouseButton button);
    bool mouseButtonHeld(MouseButton button);

    Vec2 getMousePos();

  private:
    Input();
    ~Input() = default;
};

}   // namespace Engine
