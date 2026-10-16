#pragma once

#include "Audio/Core/AudioTypes.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Engine::Audio {

// Fixed-capacity circular delay storage. All memory is created by Prepare;
// Read/Write/Advance are safe for the audio callback.
class DelayLine final {
public:
    AudioStatus Prepare(const AudioFormat& format, std::uint32_t maximumDelaySamples);
    void Reset() noexcept;

    [[nodiscard]] float Read(std::size_t channel, std::uint32_t delaySamples) const noexcept;
    void Write(std::size_t channel, float sample) noexcept;
    void Advance() noexcept;

    [[nodiscard]] std::uint32_t MaximumDelaySamples() const noexcept {
        return m_MaximumDelaySamples;
    }
    [[nodiscard]] std::size_t ChannelCount() const noexcept { return m_Channels; }
    [[nodiscard]] std::size_t MemoryBytes() const noexcept { return m_Buffer.size() * sizeof(float); }

private:
    std::size_t m_Channels = 0;
    std::uint32_t m_MaximumDelaySamples = 0;
    std::uint32_t m_Capacity = 0;
    std::uint32_t m_WriteIndex = 0;
    std::vector<float> m_Buffer;
};

} // namespace Engine::Audio
