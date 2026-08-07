#include "context/GlfwContext.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "GLFW/glfw3.h"

namespace Engine {

namespace GlfwContext {

static bool initialized = false;
static GLFWcursor* cursors[(int)CursorShape::Count] = {};
static bool cursorCreated[(int)CursorShape::Count] = {};

static void glfwDebugOutput(int error, const char* description);

static int glfwShapeOf(CursorShape shape)
{
    switch (shape) {
    case CursorShape::IBeam     : return GLFW_IBEAM_CURSOR;
    case CursorShape::Crosshair : return GLFW_CROSSHAIR_CURSOR;
    case CursorShape::Hand      : return GLFW_POINTING_HAND_CURSOR;
    case CursorShape::ResizeEW  : return GLFW_RESIZE_EW_CURSOR;
    case CursorShape::ResizeNS  : return GLFW_RESIZE_NS_CURSOR;
    case CursorShape::ResizeNWSE: return GLFW_RESIZE_NWSE_CURSOR;
    case CursorShape::ResizeNESW: return GLFW_RESIZE_NESW_CURSOR;
    case CursorShape::ResizeAll : return GLFW_RESIZE_ALL_CURSOR;
    case CursorShape::NotAllowed: return GLFW_NOT_ALLOWED_CURSOR;
    default                     : return GLFW_ARROW_CURSOR;
    }
}

void init()
{
    if (initialized) return;

    if (glfwInit() == GLFW_FALSE) {
        LOG_ERROR("couldn't initialize glfw");
        return;
    }
    glfwSetErrorCallback(glfwDebugOutput);

    initialized = true;
}

void quit()
{
    if (!initialized) return;

    for (int i = 0; i < (int)CursorShape::Count; i++) {
        if (cursors[i]) glfwDestroyCursor(cursors[i]);
        cursors[i] = nullptr;
        cursorCreated[i] = false;
    }

    glfwTerminate();
    initialized = false;
}

void* standardCursor(CursorShape shape)
{
    if (!initialized || shape >= CursorShape::Count) return nullptr;

    int index = (int)shape;
    // The created flag is separate from the pointer because a shape the platform does
    // not support returns null forever -- without it every frame over that node would
    // retry the creation and log an error.
    if (!cursorCreated[index]) {
        cursors[index] = glfwCreateStandardCursor(glfwShapeOf(shape));
        cursorCreated[index] = true;
    }
    return cursors[index];
}

void pollEvents()
{
    if (initialized) glfwPollEvents();
}

static void glfwDebugOutput(int error, const char* description)
{
    LOG_ERROR("[GLFW]: {}, {}", error, description);
}

};   // namespace GlfwContext

}   // namespace Engine
