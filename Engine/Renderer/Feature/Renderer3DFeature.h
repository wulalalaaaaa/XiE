#pragma once

#include "IRenderFeature.h"

namespace Engine {

class Renderer3DFeature final : public IRenderFeature {
public:
    bool Init(IRenderBackend& backend, GLFWwindow* windowHandle) override;
    void OnUpdate(IRenderBackend& backend, GLFWwindow* windowHandle, float dt) override;
    void OnRender(IRenderBackend& backend, GLFWwindow* windowHandle) override;
    void Shutdown(IRenderBackend& backend) override;
};

} // namespace Engine
