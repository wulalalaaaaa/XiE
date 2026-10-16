#pragma once

#include "Input/InputEvents.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

namespace Engine {

struct InputEventQueueStats {
    std::uint64_t pushed = 0;
    std::uint64_t drained = 0;
    std::uint64_t coalescedPointerMoves = 0;
    std::uint64_t evictedPointerMoves = 0;
    std::uint64_t droppedPointerMoves = 0;
    std::uint64_t softCapacityGrowths = 0;
    std::size_t maximumDepth = 0;
};

// One queue belongs to exactly one window; coordinates are always logical pixels.
class InputEventQueue {
public:
    explicit InputEventQueue(WindowHandle window, std::size_t capacity = 256);

    [[nodiscard]] bool Push(InputEvent event);
    [[nodiscard]] std::vector<InputEvent> Drain();
    void DiscardForHiddenWindow();
    void Clear();

    [[nodiscard]] bool Empty() const;
    [[nodiscard]] std::size_t Size() const;
    [[nodiscard]] InputEventQueueStats Stats() const;
    void ResetStats();
    [[nodiscard]] WindowHandle Window() const noexcept { return m_Window; }

private:
    WindowHandle m_Window{};
    std::size_t m_Capacity = 256;
    mutable std::mutex m_Mutex;
    std::deque<InputEvent> m_Events;
    InputEventQueueStats m_Stats{};
};

} // namespace Engine
