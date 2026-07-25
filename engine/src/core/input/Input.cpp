#include "core/input/Input.hpp"
#include "core/window_management/ViewContext.hpp"
#include "core/window_management/WindowManager.hpp"
#include <cstring>

namespace Engine {

void Input::addAxis(const std::string& name, const InputAxis& axis)
{
    m_axises.insert_or_assign(name, axis);
}
int Input::getAxis(const std::string& name) { return m_axises.at(name).getValue(); }

void Input::update()
{
    if (!WindowManager::get().anyWindowOpen()) return;

    IdType windowId = ViewContext::get().getActiveWindowId();
    Window* window = WindowManager::get().getWindow(windowId);
    if (window == nullptr) {
        windowId = WindowManager::get().getMainWindowId();
        window = WindowManager::get().getMainWindow();
    }
    if (window == nullptr) return;

    WindowState& state = m_windowStates[windowId];
    m_currState = &state;

    memcpy(state.keyPrevState, state.keyCurrState, KEY_COUNT * sizeof(bool));
    memcpy(state.buttonPrevState, state.buttonCurrState, BUTTON_COUNT * sizeof(bool));

    for (int i = 0; i < KEY_COUNT; i++)
        state.keyCurrState[i] = glfwGetKey(window->getGlfwHandle(), glfwKeyCodes[i]);

    for (int i = 0; i < BUTTON_COUNT; i++)
        state.buttonCurrState[i] = glfwGetMouseButton(window->getGlfwHandle(), i);

    double mouseX, mouseY;
    glfwGetCursorPos(window->getGlfwHandle(), &mouseX, &mouseY);
    state.mousePos.x = mouseX - window->getWidth() / 2.f;
    state.mousePos.y = window->getHeight() / 2.f - mouseY;
}

bool Input::keyPressed(KeyCode key)
{
    if (!m_currState) return false;
    return !m_currState->keyPrevState[(int)key] && m_currState->keyCurrState[(int)key];
}
bool Input::keyReleased(KeyCode key)
{
    if (!m_currState) return false;
    return m_currState->keyPrevState[(int)key] && !m_currState->keyCurrState[(int)key];
}
bool Input::keyHeld(KeyCode key)
{
    if (!m_currState) return false;
    return m_currState->keyCurrState[(int)key];
}

bool Input::mouseButtonPressed(MouseButton button)
{
    if (!m_currState) return false;
    return !m_currState->buttonPrevState[(int)button] && m_currState->buttonCurrState[(int)button];
}
bool Input::mouseButtonReleased(MouseButton button)
{
    if (!m_currState) return false;
    return m_currState->buttonPrevState[(int)button] && !m_currState->buttonCurrState[(int)button];
}
bool Input::mouseButtonHeld(MouseButton button)
{
    if (!m_currState) return false;
    return m_currState->buttonCurrState[(int)button];
}

Vec2 Input::getMousePos()
{
    if (!m_currState) return VEC2_ZERO;
    return m_currState->mousePos;
}

}   // namespace Engine
