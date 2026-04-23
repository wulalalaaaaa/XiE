#pragma once

struct GLFWwindow;

namespace Engine {

class IRenderBackend;

class IRenderFeature {
public:
    virtual ~IRenderFeature() = default;

    virtual bool Init(IRenderBackend& backend, GLFWwindow* windowHandle) = 0;
    virtual void OnUpdate(IRenderBackend& backend, GLFWwindow* windowHandle, float dt) = 0;
    virtual void OnRender(IRenderBackend& backend, GLFWwindow* windowHandle) = 0;
    virtual void Shutdown(IRenderBackend& backend) = 0;
};

} // namespace Engine
