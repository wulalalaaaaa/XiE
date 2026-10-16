#pragma once

#include "Audio/DSP/BasicProcessors.h"
#include "Audio/DSP/Diffusion/DiffusionNetwork.h"
#include "Audio/DSP/Filter/DampingFilter.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace Engine::Audio {

struct SpatialHazeConfig {
    bool enabled = false;
    float amount = 0.30f;
    float size = 0.35f;
    float diffusion = 0.50f;
    float damping = 0.40f;
    float width = 0.70f;
    float wet = 0.12f;
};

[[nodiscard]] SpatialHazeConfig ClampSpatialHazeConfig(SpatialHazeConfig config) noexcept;

struct SpatialHazeSnapshot {
    SpatialHazeConfig config{};
    std::uint64_t revision = 0;
};

[[nodiscard]] std::shared_ptr<const SpatialHazeSnapshot> BuildSpatialHazeSnapshot(
    SpatialHazeConfig config,
    std::uint64_t revision);

struct SpatialHazeMetrics {
    bool enabled = false;
    bool bypassed = false;
    float amount = 0.0f;
    float size = 0.0f;
    float diffusion = 0.0f;
    float damping = 0.0f;
    float width = 0.0f;
    float wet = 0.0f;
    std::uint64_t snapshotRevision = 0;
    std::uint64_t processedFrames = 0;
    double effectiveTailMilliseconds = 0.0;
    std::uint32_t algorithmicLatencyFrames = 0;
    std::size_t delayMemoryBytes = 0;
    double processMilliseconds = 0.0;
    double maximumProcessMilliseconds = 0.0;
};

class SpatialHazeProcessor final : public ProcessorBase {
public:
    AudioStatus Prepare(const AudioFormat& format) override;
    void Process(AudioProcessContext& context) noexcept override;
    void Reset() noexcept override;
    [[nodiscard]] const char* Name() const noexcept override { return "SpatialHaze"; }

    AudioStatus Publish(std::shared_ptr<const SpatialHazeSnapshot> snapshot);
    [[nodiscard]] SpatialHazeMetrics CaptureMetrics() const noexcept;
    [[nodiscard]] std::uint32_t LatencyFrames() const noexcept { return 0; }

private:
    void AdoptSnapshot(const SpatialHazeSnapshot* snapshot) noexcept;

    AudioFormat m_Format{};
    DiffusionNetwork m_Network;
    DampingFilter m_OutputDamping;
    std::vector<std::shared_ptr<const SpatialHazeSnapshot>> m_Retained;
    std::atomic<const SpatialHazeSnapshot*> m_Current{nullptr};
    const SpatialHazeSnapshot* m_AudioSnapshot = nullptr;
    ParameterSmoother m_EnableMix;
    ParameterSmoother m_Amount;
    ParameterSmoother m_Size;
    ParameterSmoother m_Diffusion;
    ParameterSmoother m_Damping;
    ParameterSmoother m_Width;
    ParameterSmoother m_Wet;
    std::atomic<bool> m_Enabled{false};
    std::atomic<std::uint64_t> m_Revision{0};
    std::atomic<std::uint64_t> m_ProcessedFrames{0};
    std::atomic<float> m_CurrentAmount{0.0f};
    std::atomic<float> m_CurrentSize{0.35f};
    std::atomic<float> m_CurrentDiffusion{0.50f};
    std::atomic<float> m_CurrentDamping{0.40f};
    std::atomic<float> m_CurrentWidth{0.70f};
    std::atomic<float> m_CurrentWet{0.12f};
    std::atomic<double> m_EffectiveTailMilliseconds{0.0};
    std::atomic<double> m_ProcessMilliseconds{0.0};
    std::atomic<double> m_MaximumProcessMilliseconds{0.0};
    std::uint32_t m_MaximumDelaySamples = 1;
    bool m_Prepared = false;
};

} // namespace Engine::Audio
