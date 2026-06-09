#pragma once

#include "math/Vec2.hpp"

class IWindow
{
  public:
    IWindow() = default;
    virtual ~IWindow() = default;

    virtual void pollEvents() = 0;
    virtual bool isOpen() const = 0;
    virtual void* getNativeHandle() const = 0;

    virtual Vec2 getPosition() const = 0;
    virtual Vec2 getSize() const = 0;
    virtual std::string getTitle() const = 0;
    virtual void centerOnScreen() = 0;

    virtual void setPosition(int x, int y) = 0;
    virtual void setSize(int width, int height) = 0;
    virtual void setPosition(Vec2 position) = 0;
    virtual void setSize(Vec2 size) = 0;
    virtual void setTitle(const std::string& title) = 0;

    virtual void fullscreen(bool enable = true) = 0;
    virtual void minimize() = 0;
    virtual void restore() = 0;

    virtual void vsync(bool enable = true) = 0;
};
