#pragma once

#include "Audio/DSP/BasicProcessors.h"
#include "Audio/DSP/Diffusion/AllPassDiffuser.h"
#include "Audio/DSP/Filter/DampingFilter.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace Engine::Audio {

struct SoftHazeConfig {
    bool enabled = false;
    float amount = 0.35f;
    float blur = 0.35f;
    float damping = 0.35f;
    float wet = 0.20f;
};

[[nodiscard]] SoftHazeConfig ClampSoftHazeConfig(SoftHazeConfig config) noexcept;

struct SoftHazeSnapshot {
    SoftHazeConfig config{};
    std::uint64_t revision = 0;
};

[[nodiscard]] std::shared_ptr<const SoftHazeSnapshot> BuildSoftHazeSnapshot(
    SoftHazeConfig config,
    std::uint64_t revision);

struct SoftHazeMetrics {
    bool enabled = false;
    bool bypassed = false;
    float amount = 0.0f;
    float blur = 0.0f;
    float damping = 0.0f;
    float wet = 0.0f;
    std::uint64_t snapshotRevision = 0;
    std::uint64_t processedFrames = 0;
    std::uint32_t effectiveDelaySamples = 0;
    double effectiveDelayMilliseconds = 0.0;
    std::uint32_t algorithmicLatencyFrames = 0;
    std::size_t delayMemoryBytes = 0;
    double processMilliseconds = 0.0;
    double maximumProcessMilliseconds = 0.0;
};

class SoftHazeProcessor final : public ProcessorBase {
public:
    AudioStatus Prepare(const AudioFormat& format) override;
    void Process(AudioProcessContext& context) noexcept override;
    void Reset() noexcept override;
    [[nodiscard]] const char* Name() const noexcept override { return "SoftHaze"; }

    AudioStatus Publish(std::shared_ptr<const SoftHazeSnapshot> snapshot);
    [[nodiscard]] SoftHazeMetrics CaptureMetrics() const noexcept;
    [[nodiscard]] std::uint32_t LatencyFrames() const noexcept { return 0; }

private:
    void AdoptSnapshot(const SoftHazeSnapshot* snapshot) noexcept;

    AudioFormat m_Format{};
    AllPassDiffuser m_Diffuser;
    DampingFilter m_DampingFilter;
    std::vector<std::shared_ptr<const SoftHazeSnapshot>> m_Retained;
    std::atomic<const SoftHazeSnapshot*> m_Current{nullptr};
    const SoftHazeSnapshot* m_AudioSnapshot = nullptr;
    ParameterSmoother m_EnableMix;
    ParameterSmoother m_Amount;
    ParameterSmoother m_Blur;
    ParameterSmoother m_Damping;
    ParameterSmoother m_Wet;
    std::atomic<bool> m_Enabled{false};
    std::atomic<std::uint64_t> m_Revision{0};
    std::atomic<std::uint64_t> m_ProcessedFrames{0};
    std::atomic<std::uint32_t> m_EffectiveDelaySamples{0};
    std::atomic<float> m_CurrentAmount{0.0f};
    std::atomic<float> m_CurrentBlur{0.35f};
    std::atomic<float> m_CurrentDamping{0.35f};
    std::atomic<float> m_CurrentWet{0.20f};
    std::atomic<double> m_ProcessMilliseconds{0.0};
    std::atomic<double> m_MaximumProcessMilliseconds{0.0};
    std::uint32_t m_MaximumDelaySamples = 1;
    bool m_Prepared = false;
};

} // namespace Engine::Audio
