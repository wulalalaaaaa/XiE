#include "Window.h"

#include <stdexcept>

#include <GLFW/glfw3.h>

namespace Engine {

Window::Window(int width, int height, const char* title) {
    m_Handle = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!m_Handle) {
        throw std::runtime_error("Failed to create GLFW window");
    }
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
}

void Window::SwapBuffers() const {
    glfwSwapBuffers(m_Handle);
}

GLFWwindow* Window::GetNativeHandle() const {
    return m_Handle;
}

} // namespace Engine
