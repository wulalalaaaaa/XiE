#pragma once

#include "Audio/DSP/Noise/NoiseSuppressorTypes.h"

namespace Engine::Audio {

class INoiseSuppressor {
public:
    virtual ~INoiseSuppressor() = default;

    virtual AudioStatus Prepare(const NoiseSuppressorPrepareInfo& info) = 0;
    virtual void Reset() noexcept = 0;
    [[nodiscard]] virtual std::size_t RequiredFrameSize() const noexcept = 0;
    [[nodiscard]] virtual std::uint32_t RequiredSampleRate() const noexcept = 0;
    virtual NoiseFrameResult ProcessFrame(
        const float* input,
        float* output,
        std::size_t frames) noexcept = 0;
};

} // namespace Engine::Audio
