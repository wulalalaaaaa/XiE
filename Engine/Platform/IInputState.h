#pragma once

namespace Engine {

enum class KeyCode {
    Unknown = -1,
    Space = 32,
    A = 65,
    D = 68,
    S = 83,
    W = 87,
    Escape = 256,
    Right = 262,
    Left = 263,
    Down = 264,
    Up = 265,
    F5 = 294,
    F6 = 295,
    F7 = 296
};

class IInputState {
public:
    virtual ~IInputState() = default;

    virtual bool IsKeyDown(KeyCode key) const = 0;
};

} // namespace Engine
