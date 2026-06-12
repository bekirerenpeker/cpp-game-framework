#include "EngineInclude.hpp"

int main()
{
    Window window(1000, 800, "Terraria Clone");

    while (window.isOpen()) {
        window.swapBuffers();
        GlfwContext::pollEvents();
    }

    GlfwContext::quit();
    return 0;
}
