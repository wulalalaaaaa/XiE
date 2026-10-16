#pragma once

#include "Audio/Core/AudioTypes.h"
#include "Audio/DSP/Delay/DelayLine.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Engine::Audio {

class AllPassDiffuser final {
public:
    AudioStatus Prepare(const AudioFormat& format, std::uint32_t maximumDelaySamples);
    void SetParameters(std::uint32_t delaySamples, float feedback, float damping) noexcept;
    [[nodiscard]] float ProcessChannel(std::size_t channel, float input) noexcept;
    void Advance() noexcept { m_Delay.Advance(); }
    void Reset() noexcept;

    [[nodiscard]] std::uint32_t DelaySamples() const noexcept { return m_DelaySamples; }
    [[nodiscard]] std::size_t MemoryBytes() const noexcept { return m_Delay.MemoryBytes(); }

private:
    DelayLine m_Delay;
    std::uint32_t m_DelaySamples = 1;
    float m_Feedback = 0.35f;
    float m_Damping = 0.0f;
    std::vector<float> m_DampingState;
};

} // namespace Engine::Audio
