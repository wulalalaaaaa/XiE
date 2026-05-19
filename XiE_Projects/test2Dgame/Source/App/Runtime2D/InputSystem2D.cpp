#include "InputSystem2D.h"

#include <GLFW/glfw3.h>

#include <cmath>

namespace Test2D {

void InputSystem2D::Tick(GLFWwindow* window, World2D& world) const {
    if (window == nullptr) {
        return;
    }

    Entity2D* player = world.FindEntity(world.GetPlayer());
    if (player == nullptr) {
        return;
    }

    float axisX = 0.0f;
    float axisY = 0.0f;

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
        axisX -= 1.0f;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        axisX += 1.0f;
    }
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        axisY += 1.0f;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        axisY -= 1.0f;
    }

    if (axisX != 0.0f || axisY != 0.0f) {
        const float length = std::sqrt(axisX * axisX + axisY * axisY);
        if (length > 0.0f) {
            axisX /= length;
            axisY /= length;
        }
    }

    player->velocity.linear.x = axisX * m_MoveSpeed;
    player->velocity.linear.y = axisY * m_MoveSpeed;
}

void InputSystem2D::SetMoveSpeed(float unitsPerSecond) {
    if (unitsPerSecond > 0.0f) {
        m_MoveSpeed = unitsPerSecond;
    }
}

} // namespace Test2D
