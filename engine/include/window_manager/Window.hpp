#pragma once

#include "GLFW/glfw3.h"
#include "math/Mat4.hpp"
#include "math/Vec2.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>

#define WINDOW_HINT_NOT_RESIZABLE      1 << 0    // disbale resizing of window
#define WINDOW_HINT_NOT_VISIBLE        1 << 1    // make the window unvisible
#define WINDOW_HINT_NOT_DECORATED      1 << 2    // remove the border and the topbar
#define WINDOW_HINT_NOT_FOCUSED        1 << 3    // dont focus on the window upon creation
#define WINDOW_HINT_DONT_AUTO_ICONIFY  1 << 4    // full screen window will not iconify
#define WINDOW_HINT_ALWAYS_ON_TOP      1 << 5    // make window always on topwin
#define WINDOW_HINT_MAXIMIZED          1 << 6    // make the window windowed fullscreen
#define WINDOW_HINT_TRANSPARENT        1 << 8    // allow the window to be transparent
#define WINDOW_HINT_DONT_CENTER_CURSOR 1 << 7    // dont center the cursor on the window
#define WINDOW_HINT_DONT_FOCUS_ON_SHOW 1 << 9    // dont focus on the window when show is called
#define WINDOW_HINT_SCALE_TO_MONITOR   1 << 10   // use the monitors scale settings

class Window
{
  private:
    static std::unordered_map<GLFWwindow*, Window*> s_CreatedWindows;

    GLFWwindow* m_GlfwHandle = nullptr;
    int m_Width = 0, m_Height = 0;
    int m_XPos = 0, m_YPos = 0;
    std::string m_Title = "";

  public:
    Window(int width, int height, const std::string& title, uint16_t creationHints = 0);
    ~Window();

    void swapBuffers();
    bool isOpen();

    Mat4 getProjMat() const;

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

    const GLFWwindow* getGlfwHandle() const { return m_GlfwHandle; }
    GLFWwindow* getGlfwHandle() { return m_GlfwHandle; }
    int getWidth() const { return m_Width; }
    int getHeight() const { return m_Height; }
    float getAspectRatio() const { return m_Width / (float)m_Height; }
    Vec2 getSize() const { return Vec2(m_Width, m_Height); }
    int getXPos() const { return m_XPos; }
    int getYPos() const { return m_YPos; }
    std::string getTitle() const { return m_Title; }

  private:
    static void sizeUpdateCallback(GLFWwindow* glfwHandle, int width, int height);
    static void positionUpdateCallback(GLFWwindow* glfwHandle, int x, int y);
};
