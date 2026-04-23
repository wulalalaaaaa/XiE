#include "Camera2DController.h"

#include "Camera2D.h"

#include <GLFW/glfw3.h>

namespace Engine {

namespace {

double g_ScrollDeltaY = 0.0;

void ScrollCallback(GLFWwindow*, double, double yoffset) {
    g_ScrollDeltaY += yoffset;
}

} // namespace

void Camera2DController::OnUpdate(GLFWwindow* window, Camera2D& camera, float dt) const {
    if (window == nullptr) {
        return;
    }

    glfwSetScrollCallback(window, ScrollCallback);

    float moveX = 0.0f;
    float moveY = 0.0f;

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        moveX -= 1.0f;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        moveX += 1.0f;
    }
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        moveY += 1.0f;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        moveY -= 1.0f;
    }

    camera.Move(moveX * m_MoveSpeed * dt, moveY * m_MoveSpeed * dt);

    if (g_ScrollDeltaY != 0.0) {
        camera.AddZoom(static_cast<float>(g_ScrollDeltaY) * m_ZoomSpeed * dt * 10.0f);
        g_ScrollDeltaY = 0.0;
    }
}

} // namespace Engine
