#pragma once

struct GLFWwindow;

namespace Engine {

class IRenderBackend {
public:
    virtual ~IRenderBackend() = default;

    virtual bool Init(GLFWwindow* windowHandle) = 0;
    virtual void SetViewport(int width, int height) = 0;
    virtual void UploadTriangleVertices(const float* vertices, int floatCount) = 0;
    virtual void BeginFrame() = 0;
    virtual void DrawTriangle() = 0;
    virtual void EndFrame() = 0;
    virtual void Shutdown() = 0;
};

} // namespace Engine
