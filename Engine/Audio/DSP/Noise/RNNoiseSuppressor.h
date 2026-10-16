#pragma once

#include "Audio/DSP/Noise/INoiseSuppressor.h"

#include <cstdint>
#include <vector>

struct DenoiseState;

namespace Engine::Audio {

class RNNoiseSuppressor final : public INoiseSuppressor {
public:
    RNNoiseSuppressor() = default;
    ~RNNoiseSuppressor() override;

    RNNoiseSuppressor(const RNNoiseSuppressor&) = delete;
    RNNoiseSuppressor& operator=(const RNNoiseSuppressor&) = delete;

    AudioStatus Prepare(const NoiseSuppressorPrepareInfo& info) override;
    void Reset() noexcept override;
    [[nodiscard]] std::size_t RequiredFrameSize() const noexcept override;
    [[nodiscard]] std::uint32_t RequiredSampleRate() const noexcept override { return 48'000; }
    NoiseFrameResult ProcessFrame(
        const float* input, float* output, std::size_t frames) noexcept override;

private:
    DenoiseState* m_State = nullptr;
    std::vector<float> m_ScaledInput;
    std::vector<float> m_ScaledOutput;
    std::size_t m_FrameSize = 0;
};

} // namespace Engine::Audio
