#include "core/input/InputAxis.hpp"
#include "core/input/Input.hpp"
#include "utils/math/MathFuncs.hpp"

namespace Engine {

int InputAxis::getValue()
{
    int value = 0;

    if (pKey != KeyCode::Unknown) value += Input::get().keyHeld(pKey);
    if (nKey != KeyCode::Unknown) value -= Input::get().keyHeld(nKey);

    if (pButton != MouseButton::Unknown) value += Input::get().mouseButtonHeld(pButton);
    if (nButton != MouseButton::Unknown) value -= Input::get().mouseButtonHeld(nButton);

    if (altPKey != KeyCode::Unknown) value += Input::get().keyHeld(altPKey);
    if (altNKey != KeyCode::Unknown) value -= Input::get().keyHeld(altNKey);

    if (altPButton != MouseButton::Unknown) value += Input::get().mouseButtonHeld(altPButton);
    if (altNButton != MouseButton::Unknown) value -= Input::get().mouseButtonHeld(altNButton);

    return Math::clamp(value, -1, 1);
}

}   // namespace Engine
