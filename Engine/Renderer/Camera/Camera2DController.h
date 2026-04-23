#pragma once

struct GLFWwindow;

namespace Engine {

class Camera2D;

class Camera2DController {
public:
    void OnUpdate(GLFWwindow* window, Camera2D& camera, float dt) const;

private:
    float m_MoveSpeed = 400.0f;
    float m_ZoomSpeed = 1.2f;
};

} // namespace Engine
