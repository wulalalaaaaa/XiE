#include "Renderer3DFeature.h"

#include "Renderer/IRenderBackend.h"

namespace Engine {

bool Renderer3DFeature::Init(IRenderBackend& backend, GLFWwindow* windowHandle) {
    (void)backend;
    (void)windowHandle;
    return true;
}

void Renderer3DFeature::OnUpdate(IRenderBackend& backend, GLFWwindow* windowHandle, float dt) {
    (void)backend;
    (void)windowHandle;
    (void)dt;
}

void Renderer3DFeature::OnRender(IRenderBackend& backend, GLFWwindow* windowHandle) {
    (void)windowHandle;
    backend.BeginFrame();
    backend.EndFrame();
}

void Renderer3DFeature::Shutdown(IRenderBackend& backend) {
    (void)backend;
}

} // namespace Engine
