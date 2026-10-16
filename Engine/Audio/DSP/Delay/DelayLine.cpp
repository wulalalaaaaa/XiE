#include "Audio/DSP/Delay/DelayLine.h"

#include <algorithm>

namespace Engine::Audio {

AudioStatus DelayLine::Prepare(const AudioFormat& format, std::uint32_t maximumDelaySamples) {
    if (!format.IsValid() || format.channelCount == 0 || maximumDelaySamples == 0) {
        return {AudioStatusCode::InvalidArgument, 0};
    }
    m_Channels = format.channelCount;
    m_MaximumDelaySamples = maximumDelaySamples;
    m_Capacity = maximumDelaySamples + 1;
    m_WriteIndex = 0;
    m_Buffer.assign(static_cast<std::size_t>(m_Channels) * m_Capacity, 0.0f);
    return {};
}

void DelayLine::Reset() noexcept {
    std::fill(m_Buffer.begin(), m_Buffer.end(), 0.0f);
    m_WriteIndex = 0;
}

float DelayLine::Read(std::size_t channel, std::uint32_t delaySamples) const noexcept {
    if (channel >= m_Channels || m_Capacity == 0) return 0.0f;
    const std::uint32_t delay = std::min(delaySamples, m_MaximumDelaySamples);
    const std::uint32_t index = (m_WriteIndex + m_Capacity - delay) % m_Capacity;
    return m_Buffer[channel * m_Capacity + index];
}

void DelayLine::Write(std::size_t channel, float sample) noexcept {
    if (channel >= m_Channels || m_Capacity == 0) return;
    m_Buffer[channel * m_Capacity + m_WriteIndex] = sample;
}

void DelayLine::Advance() noexcept {
    if (m_Capacity != 0) m_WriteIndex = (m_WriteIndex + 1) % m_Capacity;
}

} // namespace Engine::Audio
