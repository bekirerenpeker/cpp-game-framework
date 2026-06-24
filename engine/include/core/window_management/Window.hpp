#pragma once

#include "utils/math/Vec2.hpp"
#include "utils/IdIndexedVector.hpp"
#include "graphics/IRenderContext.hpp"
#include <GLFW/glfw3.h>
#include <cstdint>
#include <string>

namespace Engine {

namespace WindowFlags {
enum Type : uint16_t
{
    None = 0,
    NotResizable = 1 << 0,       // disbale resizing of window
    NotVisible = 1 << 1,         // make the window invisible
    NotDecorated = 1 << 2,       // remove the border and the topbar
    NotFocused = 1 << 3,         // dont focus on the window upon creation
    DontAutoIconify = 1 << 4,    // full screen window will not iconify
    AlwaysOnTop = 1 << 5,        // make window always on topwin
    Maximized = 1 << 6,          // make the window windowed fullscreen
    DontCenterCursor = 1 << 7,   // dont center the cursor on the window
    Transparent = 1 << 8,        // allow the window to be transparent
    DontFocusOnShow = 1 << 9,    // dont focus on the window when show is called
    ScaleToMonitor = 1 << 10     // use the monitors scale settings
};
}   // namespace WindowFlags

struct WindowCreationOptions
{
    int width = 800, height = 600;
    std::string title = "New Window";
    uint16_t creationHints = WindowFlags::None;
};

class Window : public IRenderContext, public IHasId
{
  private:
    GLFWwindow* m_glfwHandle = nullptr;
    int m_width = 0, m_height = 0;
    int m_xPos = 0, m_yPos = 0;
    std::string m_title = "";

    struct NoInit
    {
    };
    Window(NoInit) : m_glfwHandle(nullptr) {}

  public:
    Window(
        const WindowCreationOptions& opts, GLFWmonitor* monitor = nullptr,
        GLFWwindow* share = nullptr
    );
    ~Window();

    Window(const Window& other) = delete;
    Window& operator=(const Window& other) = delete;

    friend void swap(Window& first, Window& second) noexcept;
    Window(Window&& other) noexcept;
    Window& operator=(Window&& other) noexcept;

    void swapBuffers();
    bool isOpen();

    void fullscreen();
    void maximize();
    void restore();
    void centerOnScreen();

    void hide();
    void show();
    void focus();

    void setOpacity(float opacity = 1);
    void setSizeLimits(int minWidth, int minHeight, int maxWidth, int maxHeight);
    void setAspectRatio(int width = 16, int height = 9);

    void setTitle(const std::string& title);
    void setSize(int width, int height);
    void setPos(int x, int y);

    const GLFWwindow* getGlfwHandle() const { return m_glfwHandle; }
    GLFWwindow* getGlfwHandle() { return m_glfwHandle; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    Vec2 getSize() const { return Vec2(m_width, m_height); }
    float getAspectRatio() const { return m_height == 0 ? 0.f : m_width / (float)m_height; }
    int getXPos() const { return m_xPos; }
    int getYPos() const { return m_yPos; }
    std::string getTitle() const { return m_title; }

  protected:
    // render context overrides
    void bindRenderContext() override;
    void unbindRenderContext() override;
    int getRenderContextWidth() override;
    int getRenderContextHeight() override;

  private:
    static void sizeUpdateCallback(GLFWwindow* glfwHandle, int width, int height);
    static void positionUpdateCallback(GLFWwindow* glfwHandle, int x, int y);
};

}   // namespace Engine
