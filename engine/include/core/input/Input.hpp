#pragma once

#include "core/input/KeyCodes.hpp"
#include "core/input/InputAxis.hpp"
#include "utils/Singleton.hpp"
#include "utils/TypeAliases.hpp"
#include "utils/math/Vec2.hpp"
#include <string>
#include <string_view>
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

    struct WindowState
    {
        bool keyCurrState[KEY_COUNT] = {}, keyPrevState[KEY_COUNT] = {};
        bool keyRepeatState[KEY_COUNT] = {};
        bool buttonCurrState[BUTTON_COUNT] = {}, buttonPrevState[BUTTON_COUNT] = {};
        Vec2 mousePos = VEC2_ZERO;
        Vec2 scrollDelta = VEC2_ZERO;
        std::string typedText;
    };

    std::unordered_map<IdType, WindowState> m_windowStates;
    WindowState* m_currState = nullptr;

    std::unordered_map<std::string, InputAxis> m_axises;

    bool m_uiKeyboardCapture = false;
    bool m_uiMouseCapture = false;
    bool m_pendingUiKeyboardCapture = false;
    bool m_pendingUiMouseCapture = false;

  public:
    // The device without the UI capture filter, for whatever system is doing the
    // capturing. Never for gameplay.
    class Uncaptured
    {
        friend class Input;

      private:
        Input& m_input;
        explicit Uncaptured(Input& input) : m_input(input) {}

      public:
        bool keyPressed(KeyCode key) const { return m_input.readKeyPressed(key); }
        bool keyReleased(KeyCode key) const { return m_input.readKeyReleased(key); }
        bool keyHeld(KeyCode key) const { return m_input.readKeyHeld(key); }
        bool keyRepeated(KeyCode key) const { return m_input.readKeyRepeated(key); }

        bool mouseButtonPressed(MouseButton button) const
        {
            return m_input.readMouseButtonPressed(button);
        }
        bool mouseButtonReleased(MouseButton button) const
        {
            return m_input.readMouseButtonReleased(button);
        }
        bool mouseButtonHeld(MouseButton button) const
        {
            return m_input.readMouseButtonHeld(button);
        }

        Vec2 getMousePos() const { return m_input.readMousePos(); }
        Vec2 getScrollDelta() const { return m_input.readScrollDelta(); }
        std::string_view getTypedText() const { return m_input.readTypedText(); }
    };

    void update();

    Uncaptured uncaptured() { return Uncaptured(*this); }

    // Takes effect on the next update(), so it decays unless re-claimed every frame.
    void setUiCapture(bool keyboard, bool mouse);
    bool isKeyboardCapturedByUi() const { return m_uiKeyboardCapture; }
    bool isMouseCapturedByUi() const { return m_uiMouseCapture; }

    void addAxis(const std::string& name, const InputAxis& axis);
    int getAxis(const std::string& name);

    bool keyPressed(KeyCode key);
    bool keyReleased(KeyCode key);
    bool keyHeld(KeyCode key);
    bool keyRepeated(KeyCode key);

    bool mouseButtonPressed(MouseButton button);
    bool mouseButtonReleased(MouseButton button);
    bool mouseButtonHeld(MouseButton button);

    Vec2 getMousePos();
    Vec2 getScrollDelta();   // x right, y up, in wheel ticks
    std::string_view getTypedText();

  private:
    Input() = default;
    ~Input() = default;

    static const std::unordered_map<int, uint>& glfwKeyLookup();

    bool readKeyPressed(KeyCode key) const;
    bool readKeyReleased(KeyCode key) const;
    bool readKeyHeld(KeyCode key) const;
    bool readKeyRepeated(KeyCode key) const;
    bool readMouseButtonPressed(MouseButton button) const;
    bool readMouseButtonReleased(MouseButton button) const;
    bool readMouseButtonHeld(MouseButton button) const;
    Vec2 readMousePos() const;
    Vec2 readScrollDelta() const;
    std::string_view readTypedText() const;
};

}   // namespace Engine
