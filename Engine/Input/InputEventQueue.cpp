#include "Input/InputEventQueue.h"
#include "Input/InputDiagnostics.h"

#include <algorithm>
#include <optional>
#include <type_traits>

namespace Engine {
namespace {
InputDiagnosticKind DiagnosticKind(const InputEvent& event) {
    return std::visit([](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, PointerMoveEvent>)
            return InputDiagnosticKind::PointerMove;
        else if constexpr (std::is_same_v<T, PointerWheelEvent>)
            return InputDiagnosticKind::PointerWheel;
        else if constexpr (std::is_same_v<T, PointerLeaveEvent>)
            return InputDiagnosticKind::PointerLeave;
        else if constexpr (std::is_same_v<T, PointerButtonEvent>)
            return value.pressed ? InputDiagnosticKind::PointerDown
                                 : InputDiagnosticKind::PointerUp;
        else if constexpr (std::is_same_v<T, PointerCancelEvent>)
            return InputDiagnosticKind::PointerCancel;
        else if constexpr (std::is_same_v<T, KeyEvent>)
            return value.pressed ? InputDiagnosticKind::KeyDown
                                 : InputDiagnosticKind::KeyUp;
        else if constexpr (std::is_same_v<T, TextInputEvent>)
            return InputDiagnosticKind::TextInput;
        else if constexpr (std::is_same_v<T, TextCompositionStartEvent>)
            return InputDiagnosticKind::CompositionStart;
        else if constexpr (std::is_same_v<T, TextCompositionUpdateEvent>)
            return InputDiagnosticKind::CompositionUpdate;
        else if constexpr (std::is_same_v<T, TextCompositionCommitEvent>)
            return InputDiagnosticKind::CompositionCommit;
        else if constexpr (std::is_same_v<T, TextCompositionEndEvent>)
            return InputDiagnosticKind::CompositionEnd;
        else if constexpr (std::is_same_v<T, WindowFocusEvent>)
            return value.focused ? InputDiagnosticKind::FocusGained
                                 : InputDiagnosticKind::FocusLost;
        else
            return InputDiagnosticKind::PointerMove;
    }, event);
}
}

WindowHandle GetInputEventWindow(const InputEvent& event) {
    return std::visit([](const auto& value) { return value.window; }, event);
}

bool IsRetainableWhileHidden(const InputEvent& event) {
    const auto* focus = std::get_if<WindowFocusEvent>(&event);
    return focus != nullptr && !focus->focused;
}

InputEventQueue::InputEventQueue(WindowHandle window, std::size_t capacity)
    : m_Window(window), m_Capacity(std::max<std::size_t>(1, capacity)) {}

bool InputEventQueue::Push(InputEvent event) {
    if (!m_Window.IsValid() || GetInputEventWindow(event) != m_Window) {
        return false;
    }
    std::unique_lock lock(m_Mutex);
    ++m_Stats.pushed;
    if (std::holds_alternative<PointerMoveEvent>(event) && !m_Events.empty() &&
        std::holds_alternative<PointerMoveEvent>(m_Events.back())) {
        const auto& incoming = std::get<PointerMoveEvent>(event);
        const auto& pending = std::get<PointerMoveEvent>(m_Events.back());
        if (incoming.pointerId == pending.pointerId) {
            const PointerId pointerId = incoming.pointerId;
            m_Events.back() = std::move(event);
            ++m_Stats.coalescedPointerMoves;
            const InputDiagnosticRecord diagnostic{InputDiagnosticStage::Queue,
                InputDiagnosticKind::QueueCoalesced, m_Window,
                pointerId, m_Events.size()};
            lock.unlock();
            EmitInputDiagnostic(diagnostic);
            return true;
        }
    }
    std::optional<InputDiagnosticRecord> overflowDiagnostic;
    if (m_Events.size() >= m_Capacity) {
        if (std::holds_alternative<PointerMoveEvent>(event)) {
            const PointerMoveEvent& incoming =
                std::get<PointerMoveEvent>(event);
            const PointerId pointerId = incoming.pointerId;
            const auto staleMove = std::find_if(
                m_Events.begin(), m_Events.end(),
                [&](const InputEvent& pending) {
                    const auto* move =
                        std::get_if<PointerMoveEvent>(&pending);
                    return move && move->pointerId == incoming.pointerId;
                });
            if (staleMove != m_Events.end()) {
                m_Events.erase(staleMove);
                m_Events.push_back(std::move(event));
                ++m_Stats.coalescedPointerMoves;
                const InputDiagnosticRecord diagnostic{InputDiagnosticStage::Queue,
                    InputDiagnosticKind::QueueCoalesced, m_Window,
                    pointerId, m_Events.size()};
                lock.unlock();
                EmitInputDiagnostic(diagnostic);
                return true;
            }
            ++m_Stats.droppedPointerMoves;
            const InputDiagnosticRecord diagnostic{InputDiagnosticStage::Queue,
                InputDiagnosticKind::QueueOverflowDroppedMove,
                m_Window, m_Events.size(), m_Capacity};
            lock.unlock();
            EmitInputDiagnostic(diagnostic);
            return false;
        }
        const auto expendable = std::find_if(
            m_Events.begin(), m_Events.end(), [](const InputEvent& pending) {
                return std::holds_alternative<PointerMoveEvent>(pending);
            });
        if (expendable != m_Events.end()) {
            m_Events.erase(expendable);
            ++m_Stats.evictedPointerMoves;
            overflowDiagnostic = InputDiagnosticRecord{InputDiagnosticStage::Queue,
                InputDiagnosticKind::QueueOverflowEvictedMove,
                m_Window, m_Events.size(), m_Capacity};
        } else {
            // Capacity is a soft bound for state-machine events. Losing a
            // Down/Up, Focus or text-composition boundary is less safe than
            // bounded temporary growth until the next drain.
            ++m_Stats.softCapacityGrowths;
            overflowDiagnostic = InputDiagnosticRecord{InputDiagnosticStage::Queue,
                InputDiagnosticKind::QueueSoftCapacityGrowth,
                m_Window, m_Events.size() + 1, m_Capacity};
        }
    }
    m_Events.push_back(std::move(event));
    m_Stats.maximumDepth = std::max(m_Stats.maximumDepth, m_Events.size());
    const InputDiagnosticRecord queuedDiagnostic{InputDiagnosticStage::Queue,
        DiagnosticKind(m_Events.back()), m_Window,
        m_Events.size(), m_Capacity};
    lock.unlock();
    if (overflowDiagnostic) EmitInputDiagnostic(*overflowDiagnostic);
    EmitInputDiagnostic(queuedDiagnostic);
    return true;
}

std::vector<InputEvent> InputEventQueue::Drain() {
    std::unique_lock lock(m_Mutex);
    std::vector<InputEvent> result;
    result.reserve(m_Events.size());
    while (!m_Events.empty()) {
        result.push_back(std::move(m_Events.front()));
        m_Events.pop_front();
    }
    m_Stats.drained += result.size();
    lock.unlock();
    if (IsInputDiagnosticsEnabled()) {
        for (std::size_t i = 0; i < result.size(); ++i) {
            EmitInputDiagnostic({InputDiagnosticStage::Queue,
                InputDiagnosticKind::QueueDequeued, m_Window,
                static_cast<std::uint64_t>(DiagnosticKind(result[i])), i});
        }
    }
    return result;
}

void InputEventQueue::DiscardForHiddenWindow() {
    std::scoped_lock lock(m_Mutex);
    std::erase_if(m_Events, [](const InputEvent& event) { return !IsRetainableWhileHidden(event); });
    while (m_Events.size() > 1) {
        m_Events.pop_front();
    }
}

void InputEventQueue::Clear() { std::scoped_lock lock(m_Mutex); m_Events.clear(); }
bool InputEventQueue::Empty() const { std::scoped_lock lock(m_Mutex); return m_Events.empty(); }
std::size_t InputEventQueue::Size() const { std::scoped_lock lock(m_Mutex); return m_Events.size(); }
InputEventQueueStats InputEventQueue::Stats() const {
    std::scoped_lock lock(m_Mutex);
    return m_Stats;
}
void InputEventQueue::ResetStats() {
    std::scoped_lock lock(m_Mutex);
    m_Stats = {};
    m_Stats.maximumDepth = m_Events.size();
}

} // namespace Engine
