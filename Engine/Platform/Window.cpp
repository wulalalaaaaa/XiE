#include "Window.h"

#include <stdexcept>

#include <GLFW/glfw3.h>

namespace Engine {

Window::Window(int width, int height, const char* title) {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    m_Handle = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!m_Handle) {
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(m_Handle);
    glfwSwapInterval(1);
}

Window::~Window() {
    if (m_Handle) {
        glfwDestroyWindow(m_Handle);
        m_Handle = nullptr;
    }
}

bool Window::ShouldClose() const {
    return glfwWindowShouldClose(m_Handle) == GLFW_TRUE;
}

void Window::PollEvents() const {
    glfwPollEvents();

    // 最小闭环：按 ESC 优雅关闭窗口
    if (glfwGetKey(m_Handle, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(m_Handle, GLFW_TRUE);
    }
}

void Window::SwapBuffers() const {
    glfwSwapBuffers(m_Handle);
}

GLFWwindow* Window::GetNativeHandle() const {
    return m_Handle;
}

} // namespace Engine
