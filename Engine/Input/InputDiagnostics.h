#pragma once

#include "Platform/WindowHandle.h"

#include <cstdint>
#include <functional>

namespace Engine {

enum class InputDiagnosticStage : std::uint8_t {
    Native,
    Queue,
    Router,
    Focus,
    TextBox,
    Ime
};

enum class InputDiagnosticKind : std::uint8_t {
    PointerMove,
    PointerWheel,
    PointerLeave,
    PointerDown,
    PointerUp,
    PointerCancel,
    KeyDown,
    KeyUp,
    TextInput,
    CompositionStart,
    CompositionUpdate,
    CompositionCommit,
    CompositionEnd,
    FocusGained,
    FocusLost,
    FocusTargetChanged,
    QueueCoalesced,
    QueueOverflowEvictedMove,
    QueueOverflowDroppedMove,
    QueueSoftCapacityGrowth,
    QueueDequeued,
    HitTest,
    DroppedWhileUnfocused,
    ImeActivated,
    ImeDeactivated,
    ImeCandidatePosition
};

struct InputDiagnosticRecord {
    InputDiagnosticStage stage = InputDiagnosticStage::Native;
    InputDiagnosticKind kind = InputDiagnosticKind::PointerMove;
    WindowHandle window{};
    std::uint64_t value0 = 0;
    std::uint64_t value1 = 0;
};

using InputDiagnosticSink =
    std::function<void(const InputDiagnosticRecord&)>;

// Process-wide opt-in sink for short diagnostic sessions. With no sink
// installed (the default), no strings are formatted and no log is emitted.
void SetInputDiagnosticSink(InputDiagnosticSink sink);
[[nodiscard]] bool IsInputDiagnosticsEnabled() noexcept;
void EmitInputDiagnostic(const InputDiagnosticRecord& record);

} // namespace Engine
