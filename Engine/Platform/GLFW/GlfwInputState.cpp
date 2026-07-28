#include "GlfwInputState.h"

#include "GlfwWindowSystem.h"

#include <GLFW/glfw3.h>

namespace Engine {

GlfwInputState::GlfwInputState(const GlfwWindowSystem& windows, WindowHandle window)
    : m_Windows(&windows)
    , m_Window(window) {}

bool GlfwInputState::IsKeyDown(KeyCode key) const {
    if (m_Windows == nullptr) {
        return false;
    }

    GLFWwindow* window = m_Windows->GetGlfwWindow(m_Window);
    if (window == nullptr) {
        return false;
    }

    return glfwGetKey(window, static_cast<int>(key)) == GLFW_PRESS;
}

} // namespace Engine
