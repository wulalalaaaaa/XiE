#pragma once

#include "World2D.h"

#include <filesystem>

struct GLFWwindow;

namespace Test2D {

class InputSystem2D {
public:
    void Tick(GLFWwindow* window, World2D& world) const;
    bool LoadConfig(const std::filesystem::path& configPath);
    void SetMoveSpeed(float unitsPerSecond);

private:
    int m_MoveLeftKey = 65;      // GLFW_KEY_A
    int m_MoveRightKey = 68;     // GLFW_KEY_D
    int m_MoveUpKey = 87;        // GLFW_KEY_W
    int m_MoveDownKey = 83;      // GLFW_KEY_S
    int m_MoveLeftAltKey = 263;  // GLFW_KEY_LEFT
    int m_MoveRightAltKey = 262; // GLFW_KEY_RIGHT
    int m_MoveUpAltKey = 265;    // GLFW_KEY_UP
    int m_MoveDownAltKey = 264;  // GLFW_KEY_DOWN
    float m_MoveSpeed = 260.0f;
};

} // namespace Test2D
