#pragma once

#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>

namespace Engine {

// A small FIFO bridge between producer threads and the application/UI thread.
// The payload type is supplied by the consumer; the Engine does not know any
// Crystal or product-specific event vocabulary.
template <typename Event>
class MainThreadEventQueue {
public:
    MainThreadEventQueue() = default;
    MainThreadEventQueue(const MainThreadEventQueue&) = delete;
    MainThreadEventQueue& operator=(const MainThreadEventQueue&) = delete;

    void SetWakeCallback(std::function<void()> callback) {
        std::scoped_lock lock(m_Mutex);
        m_WakeCallback = std::move(callback);
    }

    void Enqueue(Event event) {
        std::function<void()> wake;
        {
            std::scoped_lock lock(m_Mutex);
            m_Events.push_back(std::move(event));
            wake = m_WakeCallback;
        }
        if (wake) wake();
    }

    [[nodiscard]] std::vector<Event> Drain() {
        std::scoped_lock lock(m_Mutex);
        std::vector<Event> result;
        result.reserve(m_Events.size());
        while (!m_Events.empty()) {
            result.push_back(std::move(m_Events.front()));
            m_Events.pop_front();
        }
        return result;
    }

    void Clear() {
        std::scoped_lock lock(m_Mutex);
        m_Events.clear();
    }

    [[nodiscard]] std::size_t Size() const {
        std::scoped_lock lock(m_Mutex);
        return m_Events.size();
    }

private:
    mutable std::mutex m_Mutex;
    std::deque<Event> m_Events;
    std::function<void()> m_WakeCallback;
};

} // namespace Engine
