#pragma once

struct GLFWwindow;

namespace Engine {

class Renderer {
public:
    bool Init(GLFWwindow* windowHandle);
    void BeginFrame();
    void EndFrame();
    void Shutdown();

private:
    bool InitOpenGLFunctions(GLFWwindow* windowHandle);
    bool CreateTrianglePipeline();

private:
    unsigned int m_VAO = 0;
    unsigned int m_VBO = 0;
    unsigned int m_ShaderProgram = 0;
};

} // namespace Engine
