#include "Audio/Core/AudioSpscRingBuffer.h"

#include <algorithm>

namespace Engine::Audio {

void AudioSpscRingBuffer::Prepare(std::size_t channels, std::size_t capacityFrames) {
    m_Channels = std::max<std::size_t>(1, channels);
    m_Capacity = std::max<std::size_t>(1, capacityFrames) + 1;
    m_Data.assign(m_Channels * m_Capacity, 0.0f);
    Reset();
}

void AudioSpscRingBuffer::Reset() noexcept {
    m_Read.store(0, std::memory_order_release);
    m_Write.store(0, std::memory_order_release);
}

std::size_t AudioSpscRingBuffer::Push(ConstAudioBufferView input) noexcept {
    if (m_Capacity == 0 || input.ChannelCount() == 0) {
        return 0;
    }
    std::size_t write = m_Write.load(std::memory_order_relaxed);
    const std::size_t read = m_Read.load(std::memory_order_acquire);
    std::size_t pushed = 0;
    while (pushed < input.FrameCount()) {
        const std::size_t next = (write + 1) % m_Capacity;
        if (next == read) {
            break;
        }
        for (std::size_t channel = 0; channel < m_Channels; ++channel) {
            const std::size_t source = std::min(channel, input.ChannelCount() - 1);
            m_Data[channel * m_Capacity + write] = input.Sample(source, pushed);
        }
        write = next;
        ++pushed;
    }
    m_Write.store(write, std::memory_order_release);
    return pushed;
}

std::size_t AudioSpscRingBuffer::Pop(AudioBufferView output) noexcept {
    if (m_Capacity == 0 || output.ChannelCount() == 0) {
        output.Clear();
        return 0;
    }
    std::size_t read = m_Read.load(std::memory_order_relaxed);
    const std::size_t write = m_Write.load(std::memory_order_acquire);
    std::size_t popped = 0;
    while (popped < output.FrameCount() && read != write) {
        for (std::size_t channel = 0; channel < output.ChannelCount(); ++channel) {
            const std::size_t source = std::min(channel, m_Channels - 1);
            output.Sample(channel, popped) = m_Data[source * m_Capacity + read];
        }
        read = (read + 1) % m_Capacity;
        ++popped;
    }
    for (std::size_t channel = 0; channel < output.ChannelCount(); ++channel) {
        std::fill(
            output.Channel(channel).begin() + static_cast<std::ptrdiff_t>(popped),
            output.Channel(channel).end(),
            0.0f);
    }
    m_Read.store(read, std::memory_order_release);
    return popped;
}

std::size_t AudioSpscRingBuffer::AvailableFrames() const noexcept {
    if (m_Capacity == 0) {
        return 0;
    }
    const std::size_t read = m_Read.load(std::memory_order_acquire);
    const std::size_t write = m_Write.load(std::memory_order_acquire);
    return write >= read ? write - read : m_Capacity - read + write;
}

} // namespace Engine::Audio
