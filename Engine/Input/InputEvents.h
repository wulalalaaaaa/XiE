#pragma once

#include "Foundation/Math/Types2D.h"
#include "Input/KeyCode.h"
#include "Platform/WindowHandle.h"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace Engine {

using PointerId = std::uint32_t;

enum class PointerButton : std::uint8_t {
    None,
    Left,
    Right,
    Middle,
    X1,
    X2,
    Primary = Left,
    Secondary = Right,
    Other = X1
};

enum class InputModifiers : std::uint8_t {
    None = 0,
    Shift = 1u << 0u,
    Control = 1u << 1u,
    Alt = 1u << 2u,
    Super = 1u << 3u
};

struct PointerMoveEvent {
    WindowHandle window{};
    Vec2F logicalPosition{};
    InputModifiers modifiers{};
    PointerId pointerId = 0;
    double timestampSeconds = 0.0;
};
struct PointerButtonEvent {
    WindowHandle window{};
    Vec2F logicalPosition{};
    PointerButton button = PointerButton::None;
    bool pressed = false;
    InputModifiers modifiers{};
    PointerId pointerId = 0;
    double timestampSeconds = 0.0;
};
struct PointerWheelEvent {
    WindowHandle window{};
    Vec2F logicalPosition{};
    Vec2F logicalDelta{};
    InputModifiers modifiers{};
    PointerId pointerId = 0;
    double timestampSeconds = 0.0;
};
struct PointerLeaveEvent {
    WindowHandle window{};
    PointerId pointerId = 0;
    double timestampSeconds = 0.0;
};
struct PointerCancelEvent {
    WindowHandle window{};
    PointerId pointerId = 0;
    Vec2F logicalPosition{};
    double timestampSeconds = 0.0;
};
struct KeyEvent {
    WindowHandle window{};
    InputKeyCode key = InputKeyCode::Unknown;
    bool pressed = false;
    bool repeat = false;
    InputModifiers modifiers{};
    double timestampSeconds = 0.0;
};
struct TextInputEvent {
    WindowHandle window{};
    char32_t codepoint = U'\0';
    double timestampSeconds = 0.0;
};
struct TextCompositionSelection {
    std::size_t anchor = 0;
    std::size_t active = 0;
};
struct TextCompositionStartEvent {
    WindowHandle window{};
    double timestampSeconds = 0.0;
};
struct TextCompositionUpdateEvent {
    WindowHandle window{};
    std::string preeditUtf8;
    std::size_t caret = 0;
    std::optional<TextCompositionSelection> selection;
    double timestampSeconds = 0.0;
};
struct TextCompositionCommitEvent {
    WindowHandle window{};
    std::string committedUtf8;
    double timestampSeconds = 0.0;
};
struct TextCompositionEndEvent {
    WindowHandle window{};
    double timestampSeconds = 0.0;
};
struct WindowFocusEvent {
    WindowHandle window{};
    bool focused = false;
    double timestampSeconds = 0.0;
};

using InputEvent = std::variant<
    PointerMoveEvent,
    PointerButtonEvent,
    PointerWheelEvent,
    PointerLeaveEvent,
    PointerCancelEvent,
    KeyEvent,
    TextInputEvent,
    TextCompositionStartEvent,
    TextCompositionUpdateEvent,
    TextCompositionCommitEvent,
    TextCompositionEndEvent,
    WindowFocusEvent>;

[[nodiscard]] WindowHandle GetInputEventWindow(const InputEvent& event);
[[nodiscard]] bool IsRetainableWhileHidden(const InputEvent& event);

} // namespace Engine
