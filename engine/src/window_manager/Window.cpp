#include "window_manager/Window.hpp"
#include "lib/glfw/GlfwImpl.hpp"
#include "GLFW/glfw3.h"
#include "GL/glew.h"

std::unordered_map<GLFWwindow*, Window*> Window::s_CreatedWindows;

Window::Window(int width, int height, const std::string& title, uint16_t creationHints)
{
    GlfwImpl::init();

    glfwDefaultWindowHints();
    if (creationHints & WINDOW_HINT_NOT_RESIZABLE) glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    if (creationHints & WINDOW_HINT_NOT_VISIBLE) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    if (creationHints & WINDOW_HINT_NOT_DECORATED) glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    if (creationHints & WINDOW_HINT_NOT_FOCUSED) glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
    if (creationHints & WINDOW_HINT_DONT_AUTO_ICONIFY)
        glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);
    if (creationHints & WINDOW_HINT_ALWAYS_ON_TOP) glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
    if (creationHints & WINDOW_HINT_MAXIMIZED) glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
    if (creationHints & WINDOW_HINT_DONT_CENTER_CURSOR)
        glfwWindowHint(GLFW_CENTER_CURSOR, GLFW_FALSE);
    if (creationHints & WINDOW_HINT_TRANSPARENT)
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    if (creationHints & WINDOW_HINT_DONT_FOCUS_ON_SHOW)
        glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);
    if (creationHints & WINDOW_HINT_SCALE_TO_MONITOR)
        glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

    m_GlfwHandle = glfwCreateWindow(width, height, title.c_str(), NULL, NULL);
    glfwMakeContextCurrent(m_GlfwHandle);

    s_CreatedWindows[m_GlfwHandle] = this;
    glfwSetWindowSizeCallback(m_GlfwHandle, Window::sizeUpdateCallback);
    glfwSetWindowPosCallback(m_GlfwHandle, Window::positionUpdateCallback);

    glfwGetWindowSize(m_GlfwHandle, &m_Width, &m_Height);
    glfwGetWindowPos(m_GlfwHandle, &m_XPos, &m_YPos);

    glViewport(0, 0, m_Width, m_Height);
}

Window::~Window() { glfwDestroyWindow(m_GlfwHandle); }

void Window::swapBuffers() { glfwSwapBuffers(m_GlfwHandle); }

bool Window::isOpen() { return !glfwWindowShouldClose(m_GlfwHandle); }

Mat4 Window::getProjMat() const
{
    return Mat4::ortho(-m_Width / 2.f, m_Width / 2.f, -m_Height / 2.f, m_Height / 2.f, -1.f, 1.f);
}

void Window::fullscreen()
{
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    glfwSetWindowMonitor(
        m_GlfwHandle, glfwGetPrimaryMonitor(), 0, 0, mode->width, mode->height, mode->refreshRate
    );
}

void Window::maximize() { glfwMaximizeWindow(m_GlfwHandle); }

void Window::restore() { glfwRestoreWindow(m_GlfwHandle); }

void Window::centerOnScreen()
{
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    setPos((mode->width - m_Width) / 2.f, (mode->height - m_Height) / 2.f);
}

void Window::hide() { glfwHideWindow(m_GlfwHandle); }

void Window::show() { glfwShowWindow(m_GlfwHandle); }

void Window::focus() { glfwFocusWindow(m_GlfwHandle); }

void Window::setOpacity(float opacity) { glfwSetWindowOpacity(m_GlfwHandle, opacity); }

void Window::setSizeLimits(int minWidth, int minHeight, int maxWidth, int maxHeight)
{
    glfwSetWindowSizeLimits(m_GlfwHandle, minWidth, maxHeight, maxWidth, maxHeight);
}

void Window::setAspectRatio(int width, int height)
{
    glfwSetWindowAspectRatio(m_GlfwHandle, width, height);
}

void Window::setTitle(const std::string& title)
{
    glfwSetWindowTitle(m_GlfwHandle, title.c_str());
    m_Title = title;
}

void Window::setSize(int width, int height) { glfwSetWindowSize(m_GlfwHandle, width, height); }

void Window::setPos(int x, int y) { glfwSetWindowPos(m_GlfwHandle, x, y); }

void Window::sizeUpdateCallback(GLFWwindow* glfwHandle, int width, int height)
{
    Window* window = s_CreatedWindows[glfwHandle];
    window->m_Width = width;
    window->m_Height = height;
    glViewport(0, 0, width, height);
}
void Window::positionUpdateCallback(GLFWwindow* glfwHandle, int x, int y)
{
    Window* window = s_CreatedWindows[glfwHandle];
    window->m_XPos = x;
    window->m_YPos = y;
}
