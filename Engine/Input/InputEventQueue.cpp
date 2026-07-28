#include "Input/InputEventQueue.h"

#include <algorithm>
#include <type_traits>

namespace Engine {

WindowHandle GetInputEventWindow(const InputEvent& event) {
    return std::visit([](const auto& value) { return value.window; }, event);
}

bool IsRetainableWhileHidden(const InputEvent& event) {
    return std::holds_alternative<WindowFocusEvent>(event);
}

InputEventQueue::InputEventQueue(WindowHandle window, std::size_t capacity)
    : m_Window(window), m_Capacity(std::max<std::size_t>(1, capacity)) {}

bool InputEventQueue::Push(InputEvent event) {
    if (!m_Window.IsValid() || GetInputEventWindow(event) != m_Window) {
        return false;
    }
    std::scoped_lock lock(m_Mutex);
    if (std::holds_alternative<PointerMoveEvent>(event) && !m_Events.empty() &&
        std::holds_alternative<PointerMoveEvent>(m_Events.back())) {
        const auto& incoming = std::get<PointerMoveEvent>(event);
        const auto& pending = std::get<PointerMoveEvent>(m_Events.back());
        if (incoming.pointerId == pending.pointerId) {
            m_Events.back() = std::move(event);
            return true;
        }
    }
    if (m_Events.size() == m_Capacity) {
        m_Events.pop_front();
    }
    m_Events.push_back(std::move(event));
    return true;
}

std::vector<InputEvent> InputEventQueue::Drain() {
    std::scoped_lock lock(m_Mutex);
    std::vector<InputEvent> result;
    result.reserve(m_Events.size());
    while (!m_Events.empty()) {
        result.push_back(std::move(m_Events.front()));
        m_Events.pop_front();
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

} // namespace Engine
