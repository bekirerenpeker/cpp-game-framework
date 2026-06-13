#include "core/window_management/Window.hpp"
#include "context/GlfwContext.hpp"

namespace Engine {

Window::Window(const WindowCreationOptions& opts, GLFWmonitor* monitor, GLFWwindow* share)
{
    GlfwContext::init();

    glfwDefaultWindowHints();
    // clang-format off
    if (opts.creationHints & WindowFlags::NotResizable) glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    if (opts.creationHints & WindowFlags::NotVisible) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    if (opts.creationHints & WindowFlags::NotDecorated) glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    if (opts.creationHints & WindowFlags::NotFocused) glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
    if (opts.creationHints & WindowFlags::DontAutoIconify) glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);
    if (opts.creationHints & WindowFlags::AlwaysOnTop) glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
    if (opts.creationHints & WindowFlags::Maximized) glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
    if (opts.creationHints & WindowFlags::DontCenterCursor) glfwWindowHint(GLFW_CENTER_CURSOR, GLFW_FALSE);
    if (opts.creationHints & WindowFlags::Transparent) glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    if (opts.creationHints & WindowFlags::DontFocusOnShow) glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);
    if (opts.creationHints & WindowFlags::ScaleToMonitor) glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
    // clang-format on

    m_glfwHandle = glfwCreateWindow(opts.width, opts.height, opts.title.c_str(), monitor, share);
    glfwMakeContextCurrent(m_glfwHandle);

    glfwSetWindowUserPointer(m_glfwHandle, this);
    glfwSetWindowSizeCallback(m_glfwHandle, Window::sizeUpdateCallback);
    glfwSetWindowPosCallback(m_glfwHandle, Window::positionUpdateCallback);

    glfwGetWindowSize(m_glfwHandle, &m_width, &m_height);
    glfwGetWindowPos(m_glfwHandle, &m_xPos, &m_yPos);
    m_title = opts.title;

    // glViewport(0, 0, m_width, m_height);

    // the window doesn't appear default so we have to do this
    glfwSwapBuffers(m_glfwHandle);
}

Window::~Window()
{
    if (m_glfwHandle) glfwDestroyWindow(m_glfwHandle);
}

void swap(Window& first, Window& second) noexcept
{
    using std::swap;
    swap(first.m_glfwHandle, second.m_glfwHandle);
    swap(first.m_width, second.m_width);
    swap(first.m_height, second.m_height);
    swap(first.m_xPos, second.m_xPos);
    swap(first.m_yPos, second.m_yPos);
    swap(first.m_title, second.m_title);

    // Crucial: Update the GLFW User Pointer so callbacks point to the new instance
    if (first.m_glfwHandle) glfwSetWindowUserPointer(first.m_glfwHandle, &first);
    if (second.m_glfwHandle) glfwSetWindowUserPointer(second.m_glfwHandle, &second);
}
Window::Window(Window&& other) noexcept : Window(NoInit {}) { swap(*this, other); }
Window& Window::operator=(Window&& other) noexcept
{
    swap(*this, other);
    return *this;
}

void Window::swapBuffers() { glfwSwapBuffers(m_glfwHandle); }
bool Window::isOpen() { return !glfwWindowShouldClose(m_glfwHandle); }

void Window::fullscreen()
{
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    glfwSetWindowMonitor(
        m_glfwHandle, glfwGetPrimaryMonitor(), 0, 0, mode->width, mode->height, mode->refreshRate
    );
}
void Window::maximize() { glfwMaximizeWindow(m_glfwHandle); }
void Window::restore() { glfwRestoreWindow(m_glfwHandle); }

void Window::centerOnScreen()
{
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    setPos((mode->width - m_width) / 2.f, (mode->height - m_height) / 2.f);
}

void Window::hide() { glfwHideWindow(m_glfwHandle); }
void Window::show() { glfwShowWindow(m_glfwHandle); }
void Window::focus() { glfwFocusWindow(m_glfwHandle); }

void Window::setOpacity(float opacity) { glfwSetWindowOpacity(m_glfwHandle, opacity); }

void Window::setSizeLimits(int minWidth, int minHeight, int maxWidth, int maxHeight)
{
    glfwSetWindowSizeLimits(m_glfwHandle, minWidth, minHeight, maxWidth, maxHeight);
}

void Window::setAspectRatio(int width, int height)
{
    if (width == 0 || height == 0) return;
    glfwSetWindowAspectRatio(m_glfwHandle, width, height);
}

void Window::setTitle(const std::string& title)
{
    glfwSetWindowTitle(m_glfwHandle, title.c_str());
    m_title = title;
}

void Window::setSize(int width, int height) { glfwSetWindowSize(m_glfwHandle, width, height); }

void Window::setPos(int x, int y) { glfwSetWindowPos(m_glfwHandle, x, y); }

void Window::sizeUpdateCallback(GLFWwindow* glfwHandle, int width, int height)
{
    Window* window = static_cast<Window*>(glfwGetWindowUserPointer(glfwHandle));
    if (!window) return;
    window->m_width = width;
    window->m_height = height;

    // glViewport(0, 0, width, height);
}
void Window::positionUpdateCallback(GLFWwindow* glfwHandle, int x, int y)
{
    Window* window = static_cast<Window*>(glfwGetWindowUserPointer(glfwHandle));
    if (!window) return;
    window->m_xPos = x;
    window->m_yPos = y;
}

}   // namespace Engine
