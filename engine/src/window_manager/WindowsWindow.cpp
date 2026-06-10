#include "window_manager/WindowsWindow.hpp"

#if OS_NAME == WINDOWS

WindowsWindow::WindowsWindow() {}
WindowsWindow::~WindowsWindow() {}

void WindowsWindow::pollEvents() {}
bool WindowsWindow::isOpen() const { return false; }
void* WindowsWindow::getNativeHandle() const { return nullptr; }

Vec2 WindowsWindow::getPosition() const { return VEC2_ZERO; }
Vec2 WindowsWindow::getSize() const { return VEC2_ZERO; }
std::string WindowsWindow::getTitle() const { return ""; }
void WindowsWindow::centerOnScreen() {}

void WindowsWindow::setPosition(int x, int y) {}
void WindowsWindow::setSize(int width, int height) {}
void WindowsWindow::setPosition(Vec2 position) {}
void WindowsWindow::setSize(Vec2 size) {}
void WindowsWindow::setTitle(const std::string& title) {}

void WindowsWindow::fullscreen(bool enable) {}
void WindowsWindow::minimize() {}
void WindowsWindow::restore() {}

void WindowsWindow::vsync(bool enable) {}

#endif   // OS_NAME == WINDOWS
