#pragma once

#include "core/input/KeyCodes.hpp"

namespace Engine {

struct InputAxis
{
    KeyCode pKey = KeyCode::Unknown, nKey = KeyCode::Unknown;
    KeyCode altPKey = KeyCode::Unknown, altNKey = KeyCode::Unknown;
    MouseButton pButton = MouseButton::Unknown, nButton = MouseButton::Unknown;
    MouseButton altPButton = MouseButton::Unknown, altNButton = MouseButton::Unknown;

    InputAxis(KeyCode pKey, KeyCode nKey) : pKey(pKey), nKey(nKey) {}

    InputAxis(MouseButton pButton, MouseButton nButton) : pButton(pButton), nButton(nButton) {}

    InputAxis(KeyCode pKey, KeyCode nKey, KeyCode altPKey, KeyCode altNKey)
        : pKey(pKey), nKey(nKey), altPKey(altPKey), altNKey(altNKey)
    {
    }

    InputAxis(
        MouseButton pButton, MouseButton nButton, MouseButton altPButton, MouseButton altNButton
    )
        : pButton(pButton), nButton(nButton), altPButton(altPButton), altNButton(altNButton)
    {
    }

    int getValue();
};

}   // namespace Engine
