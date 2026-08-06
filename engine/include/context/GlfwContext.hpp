#pragma once

#include <cstdint>

namespace Engine {

enum class CursorShape : uint8_t
{
    Arrow = 0,
    IBeam,
    Crosshair,
    Hand,
    ResizeEW,
    ResizeNS,
    ResizeNWSE,
    ResizeNESW,
    ResizeAll,
    NotAllowed,
    Count
};

namespace GlfwContext {

void init();
void quit();
void pollEvents();

// A glfw cursor object is per-process rather than per-window, and every one of them has
// to be destroyed before glfwTerminate -- which is why the cache lives with the code
// that owns that lifetime instead of on Window. Created on first use; null if the
// platform has no such shape, which glfwSetCursor reads as the arrow.
void* standardCursor(CursorShape shape);

};   // namespace GlfwContext

}   // namespace Engine
