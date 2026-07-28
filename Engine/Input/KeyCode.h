#pragma once

#include <cstdint>

namespace Engine {

enum class InputKeyCode : std::uint16_t {
    Unknown,
    Escape,
    Enter,
    Tab,
    Backspace,
    Space,
    Left,
    Right,
    Up,
    Down
};

} // namespace Engine
