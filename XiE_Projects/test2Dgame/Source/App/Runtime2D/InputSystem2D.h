#pragma once

#include "World2D.h"

struct GLFWwindow;

namespace Test2D {

class InputSystem2D {
public:
    void Tick(GLFWwindow* window, World2D& world) const;
    void SetMoveSpeed(float unitsPerSecond);

private:
    float m_MoveSpeed = 220.0f;
};

} // namespace Test2D
