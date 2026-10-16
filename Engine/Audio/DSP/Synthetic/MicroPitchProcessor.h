#pragma once

#include "Audio/DSP/Pitch.h"

#include <atomic>
#include <cstdint>
#include <memory>

namespace Engine::Audio {

struct MicroPitchConfig {
    bool enabled = false;
    float amount = 0.0f;
    float character = 0.0f;
    float width = 0.5f;
    float wet = 1.0f;
};

[[nodiscard]] MicroPitchConfig ClampMicroPitchConfig(MicroPitchConfig config) noexcept;

struct MicroPitchMetrics {
    bool enabled = false;
    bool bypassed = false;
    float amount = 0.0f;
    float character = 0.0f;
    float width = 0.5f;
    float wet = 1.0f;
    float detuneCents = 0.0f;
    std::uint32_t delaySamples = 0;
    std::uint32_t algorithmicLatencyFrames = 0;
    std::uint64_t processedFrames = 0;
    std::uint64_t snapshotRevision = 0;
    double processMilliseconds = 0.0;
    double maximumProcessMilliseconds = 0.0;
};

// A small parallel micro layer built from the existing real-time pitch
// shifter. The dry path is preserved; only a smoothed, bounded layer is mixed
// back, so Amount/Wet zero are exactly transparent.
class MicroPitchProcessor final : public ProcessorBase {
public:
    explicit MicroPitchProcessor(std::uint32_t windowFrames = 128) noexcept
        : m_WindowFrames(windowFrames) {}

    AudioStatus Prepare(const AudioFormat& format) override;
    void Process(AudioProcessContext& context) noexcept override;
    void Reset() noexcept override;
    [[nodiscard]] const char* Name() const noexcept override { return "MicroPitch"; }

    void SetConfig(MicroPitchConfig config, std::uint64_t revision) noexcept;
    [[nodiscard]] MicroPitchMetrics CaptureMetrics() const noexcept;
    [[nodiscard]] std::uint32_t LatencyFrames() const noexcept { return m_WindowFrames / 2; }

private:
    void CopyToScratch(AudioBufferView source, AudioBuffer& destination) noexcept;

    AudioFormat m_Format{};
    std::uint32_t m_WindowFrames = 128;
    AudioBuffer m_LeftScratch;
    AudioBuffer m_RightScratch;
    PitchShiftProcessor m_LeftShifter;
    PitchShiftProcessor m_RightShifter;
    MicroPitchConfig m_Config{};
    ParameterSmoother m_EnableMix;
    ParameterSmoother m_Amount;
    ParameterSmoother m_Character;
    ParameterSmoother m_Width;
    ParameterSmoother m_Wet;
    std::atomic<bool> m_Enabled{false};
    std::atomic<float> m_CurrentAmount{0.0f};
    std::atomic<float> m_CurrentCharacter{0.0f};
    std::atomic<float> m_CurrentWidth{0.5f};
    std::atomic<float> m_CurrentWet{1.0f};
    std::atomic<float> m_DetuneCents{0.0f};
    std::atomic<std::uint32_t> m_DelaySamples{0};
    std::atomic<std::uint64_t> m_ProcessedFrames{0};
    std::atomic<std::uint64_t> m_SnapshotRevision{0};
    std::atomic<double> m_ProcessMilliseconds{0.0};
    std::atomic<double> m_MaximumProcessMilliseconds{0.0};
    bool m_Prepared = false;
};

} // namespace Engine::Audio
