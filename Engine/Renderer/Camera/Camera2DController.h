#pragma once

#include "Platform/IInputState.h"

namespace Engine {

class Camera2D;

class Camera2DController {
public:
    void OnUpdate(const IInputState& input, Camera2D& camera, float dt) const;

private:
    float m_MoveSpeed = 400.0f;
};

} // namespace Engine
