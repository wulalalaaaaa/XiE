#pragma once

#include "Audio/Core/AudioBuffer.h"

#include <atomic>
#include <cstddef>
#include <vector>

namespace Engine::Audio {

// Prepared planar Float32 transport for one capture producer and one render
// consumer. Prepare/Reset belong to the ordinary thread; Push/Pop are RT-safe.
class AudioSpscRingBuffer {
public:
    void Prepare(std::size_t channels, std::size_t capacityFrames);
    void Reset() noexcept;

    [[nodiscard]] std::size_t Push(ConstAudioBufferView input) noexcept;
    [[nodiscard]] std::size_t Pop(AudioBufferView output) noexcept;
    [[nodiscard]] std::size_t AvailableFrames() const noexcept;
    [[nodiscard]] std::size_t CapacityFrames() const noexcept {
        return m_Capacity > 0 ? m_Capacity - 1 : 0;
    }

private:
    std::size_t m_Channels = 0;
    std::size_t m_Capacity = 0;
    std::vector<float> m_Data;
    alignas(64) std::atomic<std::size_t> m_Read{0};
    alignas(64) std::atomic<std::size_t> m_Write{0};
};

} // namespace Engine::Audio
