#pragma once

#include "Audio/Core/AudioTypes.h"
#include "Audio/DSP/Diffusion/AllPassDiffuser.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace Engine::Audio {

inline constexpr std::size_t kDiffusionNetworkMaximumStages = 4;

class DiffusionNetwork final {
public:
    AudioStatus Prepare(const AudioFormat& format, std::uint32_t maximumDelaySamples);
    void SetStageParameters(
        std::size_t stage,
        std::uint32_t delaySamples,
        float feedback,
        float damping) noexcept;
    [[nodiscard]] float ProcessChannel(std::size_t channel, float input) noexcept;
    void Advance() noexcept;
    void Reset() noexcept;

    [[nodiscard]] std::size_t MemoryBytes() const noexcept;
    [[nodiscard]] std::uint32_t MaximumDelaySamples() const noexcept {
        return m_MaximumDelaySamples;
    }

private:
    std::array<AllPassDiffuser, kDiffusionNetworkMaximumStages> m_Stages{};
    std::size_t m_StageCount = 0;
    std::uint32_t m_MaximumDelaySamples = 0;
};

} // namespace Engine::Audio
