#include "Camera2DController.h"

#include "Camera2D.h"

namespace Engine {

void Camera2DController::OnUpdate(const IInputState& input, Camera2D& camera, float dt) const {
    float moveX = 0.0f;
    float moveY = 0.0f;

    if (input.IsKeyDown(KeyCode::A)) {
        moveX -= 1.0f;
    }
    if (input.IsKeyDown(KeyCode::D)) {
        moveX += 1.0f;
    }
    if (input.IsKeyDown(KeyCode::W)) {
        moveY += 1.0f;
    }
    if (input.IsKeyDown(KeyCode::S)) {
        moveY -= 1.0f;
    }

    camera.Move(moveX * m_MoveSpeed * dt, moveY * m_MoveSpeed * dt);
}

} // namespace Engine
