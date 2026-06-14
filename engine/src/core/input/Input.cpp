#include "core/input/Input.hpp"
#include "core/window_management/WindowManager.hpp"
#include <cstring>

namespace Engine {

Input::Input()
{
    memset(m_keyCurrState, false, KEY_COUNT * sizeof(bool));
    memset(m_keyPrevState, false, KEY_COUNT * sizeof(bool));
    memset(m_buttonCurrState, false, BUTTON_COUNT * sizeof(bool));
    memset(m_buttonPrevState, false, BUTTON_COUNT * sizeof(bool));

    m_mousePos = {0, 0};
    m_axises = {};
}

void Input::addAxis(const std::string& name, const InputAxis& axis)
{
    m_axises.insert_or_assign(name, axis);
}
int Input::getAxis(const std::string& name) { return m_axises.at(name).getValue(); }

void Input::update(IdType windowId)
{
    if (!WindowManager::get().hasWindows()) return;

    Window* window = WindowManager::get().getWindow(windowId);
    if (window == nullptr) window = WindowManager::get().getMainWindow();

    memcpy(m_keyPrevState, m_keyCurrState, KEY_COUNT * sizeof(bool));
    memcpy(m_buttonPrevState, m_buttonCurrState, BUTTON_COUNT * sizeof(bool));

    for (int i = 0; i < KEY_COUNT; i++)
        m_keyCurrState[i] = glfwGetKey(window->getGlfwHandle(), glfwKeyCodes[i]);

    for (int i = 0; i < BUTTON_COUNT; i++)
        m_buttonCurrState[i] = glfwGetMouseButton(window->getGlfwHandle(), i);

    double mouseX, mouseY;
    glfwGetCursorPos(window->getGlfwHandle(), &mouseX, &mouseY);
    m_mousePos.x = mouseX - window->getWidth() / 2.f;
    m_mousePos.y = window->getHeight() / 2.f - mouseY;
}

bool Input::keyPressed(KeyCode key)
{
    return !m_keyPrevState[(int)key] && m_keyCurrState[(int)key];
}
bool Input::keyReleased(KeyCode key)
{
    return m_keyPrevState[(int)key] && !m_keyCurrState[(int)key];
}
bool Input::keyHeld(KeyCode key) { return m_keyCurrState[(int)key]; }

bool Input::mouseButtonPressed(MouseButton button)
{
    return !m_buttonPrevState[(int)button] && m_buttonCurrState[(int)button];
}
bool Input::mouseButtonReleased(MouseButton button)
{
    return m_buttonPrevState[(int)button] && !m_buttonCurrState[(int)button];
}
bool Input::mouseButtonHeld(MouseButton button) { return m_buttonCurrState[(int)button]; }

Vec2 Input::getMousePos() { return m_mousePos; }

}   // namespace Engine
