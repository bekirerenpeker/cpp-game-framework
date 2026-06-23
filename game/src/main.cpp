#include "EngineInclude.hpp"
#include "glad/glad.h"
#include "graphics/gl_wrappers/GlShader.hpp"
#include "graphics/gl_wrappers/GlTexture.hpp"
#include "utils/ImageData.hpp"
#include "utils/math/Vec2.hpp"

using namespace Engine;

struct VertexData
{
    Vec2 pos;
    Vec2 texCoords;
    Color color;
    int texIndex;
};

int main()
{
    Logger::get().addSink<FileSink>("game/output/log.txt");

    IdType windowId = WindowManager::get().createWindow({800, 800, "Graphics Test Window"});
    Window* window = WindowManager::get().getWindow(windowId);

    GlVertexArray vertexArray;
    GlBuffer indexBuffer(GlBufferType::IndexBuffer);
    GlBuffer vertexBuffer(GlBufferType::VertexBuffer);

    VertexData vertexData[] = {
        {Vec2(-0.5, -0.5f), Vec2(0, 0), Color(0, 0, 1), 0},
        {Vec2(+0.5, -0.5f), Vec2(1, 0), Color(1, 0, 0), 0},
        {Vec2(+0.5, +0.5f), Vec2(1, 1), Color(1, 1, 0), 0},
        {Vec2(-0.5, +0.5f), Vec2(0, 1), Color(0, 1, 1), 0},
    };
    unsigned int indexData[] = {0, 1, 2, 0, 2, 3};

    vertexBuffer.setData(sizeof(vertexData), vertexData, GlBufferUsage::DynamicDraw);
    indexBuffer.setData(sizeof(indexData), indexData, GlBufferUsage::StaticDraw);
    vertexArray.setLayout({
        {GlDataType::Float, 2},
        {GlDataType::Float, 2},
        {GlDataType::Float, 4},
        {  GlDataType::Int, 1},
    });

    GlTexture texture("game/assets/images/mario.png");
    GlShader shader("game/assets/shaders/QuadShader.glsl");

    while (window->isOpen()) {
        Input::get().update(windowId);
        Time::get().update();

        if (Input::get().keyPressed(KeyCode::Q)) break;

        for (int i = 0; i < 4; i++) {
            float angleRad = Time::get().currTime() + i * PI2;
            Vec2 base(1, 0);
            vertexData[i].pos = base.rotatedAround(VEC2_ZERO, angleRad) * 0.7;
        }

        glClearColor(0.3f, 0.3f, 0.5f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.bind();
        int texIndices[32];
        for (int i = 0; i < 32; i++) {
            texture.bind(i);
            texIndices[i] = i;
        }
        shader.setUniform<Mat4>("uMVP", Mat4());
        shader.setUniformArr<int>("uTextures", 32, texIndices);

        vertexArray.bind();
        indexBuffer.bind();
        vertexBuffer.setData(sizeof(vertexData), vertexData, GlBufferUsage::DynamicDraw);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        window->swapBuffers();
        GlfwContext::pollEvents();
    }

    return 0;
}
