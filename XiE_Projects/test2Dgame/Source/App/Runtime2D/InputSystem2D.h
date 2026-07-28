#pragma once

#include "World2D.h"
#include "Platform/IInputState.h"

#include <filesystem>

namespace Test2D {

class InputSystem2D {
public:
    void Tick(const Engine::IInputState* input, World2D& world) const;
    bool LoadConfig(const std::filesystem::path& configPath);
    void SetMoveSpeed(float unitsPerSecond);

private:
    Engine::KeyCode m_MoveLeftKey = Engine::KeyCode::A;
    Engine::KeyCode m_MoveRightKey = Engine::KeyCode::D;
    Engine::KeyCode m_MoveUpKey = Engine::KeyCode::W;
    Engine::KeyCode m_MoveDownKey = Engine::KeyCode::S;
    Engine::KeyCode m_MoveLeftAltKey = Engine::KeyCode::Left;
    Engine::KeyCode m_MoveRightAltKey = Engine::KeyCode::Right;
    Engine::KeyCode m_MoveUpAltKey = Engine::KeyCode::Up;
    Engine::KeyCode m_MoveDownAltKey = Engine::KeyCode::Down;
    float m_MoveSpeed = 260.0f;
};

} // namespace Test2D
