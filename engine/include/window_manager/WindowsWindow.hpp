#pragma once

#if OS_NAME == WINDOWS

#include "window_manager/IWindow.hpp"

class WindowsWindow : public IWindow
{
  public:
    WindowsWindow();
    ~WindowsWindow() override;

    void pollEvents() override;
    bool isOpen() const override;
    void* getNativeHandle() const override;

    Vec2 getPosition() const override;
    Vec2 getSize() const override;
    std::string getTitle() const override;
    void centerOnScreen() override;

    void setPosition(int x, int y) override;
    void setSize(int width, int height) override;
    void setPosition(Vec2 position) override;
    void setSize(Vec2 size) override;
    void setTitle(const std::string& title) override;

    void fullscreen(bool enable = true) override;
    void minimize() override;
    void restore() override;

    void vsync(bool enable = true) override;
};

#endif   // OS_NAME == WINDOWS
