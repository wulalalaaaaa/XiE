#include "GlfwOpenGLSurface.h"

#include "GlfwWindowSystem.h"

#include <GLFW/glfw3.h>

namespace Engine {

GlfwOpenGLSurface::GlfwOpenGLSurface(GlfwWindowSystem& windows, WindowHandle window)
    : m_Windows(&windows)
    , m_Window(window) {}

WindowHandle GlfwOpenGLSurface::GetWindow() const {
    return m_Window;
}

void GlfwOpenGLSurface::MakeCurrent() {
    if (m_Windows == nullptr) {
        return;
    }
    if (GLFWwindow* window = m_Windows->GetGlfwWindow(m_Window)) {
        glfwMakeContextCurrent(window);
    }
}

void GlfwOpenGLSurface::Present() {
    if (m_Windows == nullptr) {
        return;
    }
    if (GLFWwindow* window = m_Windows->GetGlfwWindow(m_Window)) {
        glfwSwapBuffers(window);
    }
}

void GlfwOpenGLSurface::Resize(int width, int height) {
    if (m_Windows != nullptr) {
        m_Windows->SetWindowSize(m_Window, width, height);
    }
}

RenderSurfaceSize GlfwOpenGLSurface::GetFramebufferSize() const {
    RenderSurfaceSize size{};
    if (m_Windows == nullptr) {
        return size;
    }
    if (GLFWwindow* window = m_Windows->GetGlfwWindow(m_Window)) {
        glfwGetFramebufferSize(window, &size.width, &size.height);
    }
    return size;
}

} // namespace Engine
