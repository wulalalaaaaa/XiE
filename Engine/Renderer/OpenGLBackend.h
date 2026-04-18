#pragma once

#include "IRenderBackend.h"

namespace Engine {

class OpenGLBackend final : public IRenderBackend {
public:
    bool Init(GLFWwindow* windowHandle) override;
    void SetViewport(int width, int height) override;
    void UploadTriangleVertices(const float* vertices, int floatCount) override;
    void BeginFrame() override;
    void DrawTriangle() override;
    void EndFrame() override;
    void Shutdown() override;

private:
    bool CreateTrianglePipeline();
    unsigned int CompileShader(unsigned int type, const char* source);

private:
    unsigned int m_VAO = 0;
    unsigned int m_VBO = 0;
    unsigned int m_ShaderProgram = 0;
};

} // namespace Engine
