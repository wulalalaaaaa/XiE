#pragma once

struct GLFWwindow;

namespace Engine {

class IRenderBackend {
public:
    virtual ~IRenderBackend() = default;

    virtual bool Init(GLFWwindow* windowHandle) = 0;
    virtual void SetViewport(int width, int height) = 0;
    virtual void UploadMesh(const float* vertices, int vertexFloatCount, const unsigned int* indices, int indexCount) = 0;
    virtual void SetViewProjection(const float* matrix4x4) = 0;
    virtual void BeginFrame() = 0;
    virtual void DrawMesh() = 0;
    virtual void EndFrame() = 0;
    virtual void Shutdown() = 0;
};

} // namespace Engine
