#pragma once

#include "Audio/DSP/Synthetic/MicroPitchProcessor.h"
#include "Audio/DSP/Synthetic/PitchStabilizer.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace Engine::Audio {

struct SyntheticVoiceConfig {
    bool enabled = false;
    float amount = 0.0f;
    float stability = 0.0f;
    float electronic = 0.0f;
    float width = 0.5f;
    float wet = 1.0f;
};

[[nodiscard]] SyntheticVoiceConfig ClampSyntheticVoiceConfig(
    SyntheticVoiceConfig config) noexcept;

struct SyntheticVoiceSnapshot {
    SyntheticVoiceConfig config{};
    std::uint64_t revision = 0;
};

[[nodiscard]] std::shared_ptr<const SyntheticVoiceSnapshot> BuildSyntheticVoiceSnapshot(
    SyntheticVoiceConfig config,
    std::uint64_t revision);

struct SyntheticVoiceMetrics {
    bool enabled = false;
    bool bypassed = false;
    float amount = 0.0f;
    float stability = 0.0f;
    float electronic = 0.0f;
    float width = 0.5f;
    float wet = 1.0f;
    float rawFrequencyHz = 0.0f;
    float stabilizedFrequencyHz = 0.0f;
    float correctionCents = 0.0f;
    float confidence = 0.0f;
    bool voiced = false;
    bool correctionHeld = false;
    float controlSmoothingMilliseconds = 0.0f;
    MicroPitchMetrics micro{};
    std::uint64_t snapshotRevision = 0;
    std::uint64_t processedFrames = 0;
    std::uint32_t algorithmicLatencyFrames = 0;
    double processMilliseconds = 0.0;
    double maximumProcessMilliseconds = 0.0;
};

class SyntheticVoiceProcessor final : public ProcessorBase {
public:
    explicit SyntheticVoiceProcessor(std::uint32_t microWindowFrames = 128) noexcept
        : m_MicroPitch(microWindowFrames) {}

    AudioStatus Prepare(const AudioFormat& format) override;
    void Process(AudioProcessContext& context) noexcept override;
    void Reset() noexcept override;
    [[nodiscard]] const char* Name() const noexcept override { return "SyntheticVoice"; }

    AudioStatus Publish(std::shared_ptr<const SyntheticVoiceSnapshot> snapshot);
    [[nodiscard]] PitchStabilizerResult UpdatePitch(
        PitchEstimate estimate,
        std::uint32_t frameCount) noexcept;
    [[nodiscard]] SyntheticVoiceMetrics CaptureMetrics() const noexcept;
    [[nodiscard]] std::uint32_t LatencyFrames() const noexcept;

private:
    void AdoptSnapshot(const SyntheticVoiceSnapshot* snapshot) noexcept;

    AudioFormat m_Format{};
    PitchStabilizer m_Stabilizer;
    MicroPitchProcessor m_MicroPitch;
    std::vector<std::shared_ptr<const SyntheticVoiceSnapshot>> m_Retained;
    std::atomic<const SyntheticVoiceSnapshot*> m_Current{nullptr};
    const SyntheticVoiceSnapshot* m_AudioSnapshot = nullptr;
    SyntheticVoiceConfig m_Config{};
    std::atomic<bool> m_Enabled{false};
    std::atomic<float> m_Amount{0.0f};
    std::atomic<float> m_Stability{0.0f};
    std::atomic<float> m_Electronic{0.0f};
    std::atomic<float> m_Width{0.5f};
    std::atomic<float> m_Wet{1.0f};
    std::atomic<float> m_RawFrequencyHz{0.0f};
    std::atomic<float> m_StabilizedFrequencyHz{0.0f};
    std::atomic<float> m_CorrectionCents{0.0f};
    std::atomic<float> m_Confidence{0.0f};
    std::atomic<bool> m_Voiced{false};
    std::atomic<bool> m_CorrectionHeld{false};
    std::atomic<std::uint64_t> m_Revision{0};
    std::atomic<std::uint64_t> m_ProcessedFrames{0};
    std::atomic<double> m_ProcessMilliseconds{0.0};
    std::atomic<double> m_MaximumProcessMilliseconds{0.0};
    bool m_Prepared = false;
};

} // namespace Engine::Audio
