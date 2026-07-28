#pragma once

#include "Platform/IRenderSurface.h"

namespace Engine {

class GlfwWindowSystem;

class GlfwOpenGLSurface final : public IRenderSurface {
public:
    GlfwOpenGLSurface(GlfwWindowSystem& windows, WindowHandle window);

    WindowHandle GetWindow() const override;
    void MakeCurrent() override;
    void Present() override;
    void Resize(int width, int height) override;
    RenderSurfaceSize GetFramebufferSize() const override;

private:
    GlfwWindowSystem* m_Windows = nullptr;
    WindowHandle m_Window;
};

} // namespace Engine
