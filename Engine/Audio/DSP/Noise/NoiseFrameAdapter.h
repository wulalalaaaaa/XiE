#pragma once

#include "Audio/DSP/Noise/INoiseSuppressor.h"

#include <cstdint>
#include <span>
#include <vector>

namespace Engine::Audio {

struct NoiseFrameAdapterMetrics {
    std::uint64_t inputFrames = 0;
    std::uint64_t outputFrames = 0;
    std::uint64_t algorithmFrames = 0;
    std::uint32_t bufferedInputFrames = 0;
    std::uint32_t bufferedOutputFrames = 0;
    std::uint32_t latencyFrames = 0;
};

class NoiseFrameAdapter {
public:
    AudioStatus Prepare(std::size_t algorithmFrameSize);
    void Reset() noexcept;

    // Startup policy: fixed-delay silence. Output remains sample-count exact;
    // the first processed sample appears algorithmFrameSize samples later.
    NoiseFrameResult Process(
        std::span<const float> monoInput,
        std::span<float> delayedDryOutput,
        std::span<float> processedOutput,
        INoiseSuppressor& suppressor) noexcept;

    [[nodiscard]] NoiseFrameAdapterMetrics CaptureMetrics() const noexcept;
    [[nodiscard]] std::size_t LatencyFrames() const noexcept { return m_FrameSize; }
    [[nodiscard]] bool Prepared() const noexcept { return m_FrameSize > 0; }

private:
    bool PopProcessed(float& value) noexcept;
    void PushProcessed(std::span<const float> values) noexcept;
    float PopDelayedDry() noexcept;
    void PushDelayedDry(float value) noexcept;

    std::vector<float> m_InputFrame;
    std::vector<float> m_AlgorithmOutput;
    std::vector<float> m_ProcessedRing;
    std::vector<float> m_DryDelay;
    std::size_t m_FrameSize = 0;
    std::size_t m_InputCount = 0;
    std::size_t m_ProcessedRead = 0;
    std::size_t m_ProcessedWrite = 0;
    std::size_t m_ProcessedCount = 0;
    std::size_t m_DryRead = 0;
    std::size_t m_DryWrite = 0;
    std::size_t m_DryCount = 0;
    std::uint64_t m_InputFrames = 0;
    std::uint64_t m_OutputFrames = 0;
    std::uint64_t m_AlgorithmFrames = 0;
};

} // namespace Engine::Audio
