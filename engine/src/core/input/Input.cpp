#include "core/input/Input.hpp"
#include "core/window_management/ViewContext.hpp"
#include "core/window_management/WindowManager.hpp"
#include <cstring>

namespace Engine {

// The reverse of glfwKeyCodes, built from that same table so the two cannot drift. A map
// rather than an array because glfw's codes run to 348 with wide gaps, and this is walked
// a handful of times a frame at most.
const std::unordered_map<int, uint>& Input::glfwKeyLookup()
{
    static const std::unordered_map<int, uint> lookup = [] {
        std::unordered_map<int, uint> result;
        result.reserve(KEY_COUNT);
        for (uint i = 0; i < KEY_COUNT; i++) result.emplace((int)glfwKeyCodes[i], i);
        return result;
    }();
    return lookup;
}

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

    // Read after the keys, so shift is this frame's. A device that already reports x is
    // left alone: only a wheel, which physically has one axis, gets remapped. Negated
    // because shift-wheel-up means left everywhere it exists.
    Vec2 scroll = window->consumeScrollDelta();
    bool shiftHeld =
        state.keyCurrState[(int)KeyCode::LeftShift] || state.keyCurrState[(int)KeyCode::RightShift];
    if (shiftHeld && scroll.x == 0.0f) {
        scroll.x = -scroll.y;
        scroll.y = 0.0f;
    }
    state.scrollDelta = scroll;

    // Both are events glfw delivered during pollEvents, so they are drained here for the
    // same reason the wheel is: they accumulate rather than being a state to poll, and a
    // frame may have collected several or none.
    state.typedText = window->consumeTypedText();

    memset(state.keyRepeatState, 0, KEY_COUNT * sizeof(bool));
    const std::unordered_map<int, uint>& lookup = glfwKeyLookup();
    for (int glfwKey : window->getRepeatedKeys()) {
        auto it = lookup.find(glfwKey);
        if (it != lookup.end()) state.keyRepeatState[it->second] = true;
    }
    window->clearRepeatedKeys();
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

bool Input::keyRepeated(KeyCode key)
{
    if (!m_currState) return false;
    return m_currState->keyRepeatState[(int)key];
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

Vec2 Input::getScrollDelta()
{
    if (!m_currState) return VEC2_ZERO;
    return m_currState->scrollDelta;
}

std::string_view Input::getTypedText()
{
    if (!m_currState) return {};
    return m_currState->typedText;
}

}   // namespace Engine
