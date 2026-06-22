#include "EngineInclude.hpp"
#include "ecs_test.hpp"
#include "file_management_test.hpp"
#include "piano_demo.hpp"
#include "glad/glad.h"

using namespace Engine;

int main()
{
    Logger::get().addSink<FileSink>("game/output/log.txt");

    IdType windowId = WindowManager::get().createWindow({1000, 800, "Graphics Test Window"});
    Window* window = WindowManager::get().getWindow(windowId);

    GlVertexArray vertexArray;
    GlBuffer indexBuffer(GlBufferType::IndexBuffer);
    GlBuffer vertexBuffer(GlBufferType::VertexBuffer);

    // clang-format off
    float vertexData[] = {
        -0.5, -0.5f, 0,   1, 0, 0, 0, // v1
        +0.5, -0.5f, 0,   0, 1, 0, 0, // v2
        -0.0, +0.5f, 0,   0, 0, 1, 0, // v3
    };
    unsigned int indexData[] = {0, 1, 2};
    // clang-format on

    vertexBuffer.setData(sizeof(vertexData), vertexData, GlBufferUsage::StaticDraw);
    indexBuffer.setData(sizeof(indexData), indexData, GlBufferUsage::StaticDraw);
    vertexArray.setLayout({
        {GlDataType::Float, 3},
        {GlDataType::Float, 4}
    });

    while (window->isOpen()) {
        Input::get().update(windowId);
        Time::get().update();

        if (Input::get().keyPressed(KeyCode::Q)) break;

        glClearColor(0.3f, 0.3f, 0.5f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        vertexArray.bind();
        indexBuffer.bind();
        glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, 0);

        window->swapBuffers();
        GlfwContext::pollEvents();
    }

    return 0;
}
