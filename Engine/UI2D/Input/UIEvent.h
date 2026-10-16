#pragma once

#include "Foundation/Math/Types2D.h"
#include "Input/InputEvents.h"
#include "Input/KeyCode.h"
#include "Platform/WindowHandle.h"
#include "UI2D/Core/UINodeHandle.h"
#include "UI2D/Input/UIEventTypes.h"
#include "UI2D/Input/UIFocusTypes.h"

#include <cstdint>
#include <optional>
#include <string>

namespace Engine::UI2D {

struct UIEvent {
    UIEventType type = UIEventType::PointerMove;
    UIEventPhase phase = UIEventPhase::Target;
    Engine::WindowHandle window{};
    UINodeHandle target{};
    UINodeHandle currentTarget{};
    UINodeHandle relatedTarget{};
    // Composite control responsible for activation. target remains the exact
    // hit-test leaf so hover, routing and local hit semantics are unchanged.
    UINodeHandle interactionOwner{};
    UIFocusReason focusReason = UIFocusReason::Programmatic;
    Engine::PointerId pointerId = 0;
    Engine::PointerButton button = Engine::PointerButton::None;
    Engine::Vec2F scenePosition{};
    Engine::Vec2F localPosition{};
    Engine::Vec2F wheelDelta{};
    Engine::InputKeyCode key = Engine::InputKeyCode::Unknown;
    char32_t codepoint = U'\0';
    std::string text;
    std::size_t textCaret = 0;
    std::optional<Engine::TextCompositionSelection>
        textCompositionSelection;
    Engine::InputModifiers modifiers = Engine::InputModifiers::None;
    double timestampSeconds = 0.0;
    bool repeat = false;
    bool handled = false;
    bool defaultPrevented = false;
    bool propagationStopped = false;
    bool immediatePropagationStopped = false;
};

struct UIInputDispatchResult {
    std::uint32_t consumedInputCount = 0;
    std::uint32_t dispatchedEventCount = 0;
    std::uint32_t hitTestCount = 0;
    std::uint32_t hoverTransitionCount = 0;
    std::uint32_t clickCount = 0;
    bool interactionStateChanged = false;
    bool visualInvalidated = false;
    bool mutationQueued = false;
    bool reentrantDispatchRejected = false;
};

} // namespace Engine::UI2D
