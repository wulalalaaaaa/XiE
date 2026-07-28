#pragma once

#include <cstdint>

namespace Engine::UI2D {

enum class UIEventType : std::uint8_t {
    PointerEnter,
    PointerLeave,
    PointerMove,
    PointerDown,
    PointerUp,
    PointerCancel,
    PointerWheel,
    Click,
    KeyDown,
    KeyUp,
    TextInput,
    FocusChanging,
    FocusLost,
    FocusGained,
    WindowFocusGained,
    WindowFocusLost
};

enum class UIEventPhase : std::uint8_t { Preview, Target, Bubble };

enum class UIEventPhaseMask : std::uint8_t {
    None = 0,
    Preview = 1u << 0u,
    Target = 1u << 1u,
    Bubble = 1u << 2u,
    All = (1u << 0u) | (1u << 1u) | (1u << 2u)
};

constexpr UIEventPhaseMask operator|(UIEventPhaseMask lhs, UIEventPhaseMask rhs) noexcept {
    return static_cast<UIEventPhaseMask>(
        static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
}

constexpr bool IncludesPhase(UIEventPhaseMask mask, UIEventPhase phase) noexcept {
    const UIEventPhaseMask bit = phase == UIEventPhase::Preview ? UIEventPhaseMask::Preview
        : phase == UIEventPhase::Target ? UIEventPhaseMask::Target
        : UIEventPhaseMask::Bubble;
    return (static_cast<std::uint8_t>(mask) & static_cast<std::uint8_t>(bit)) != 0;
}

} // namespace Engine::UI2D
