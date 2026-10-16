#pragma once

#include "Audio/Core/AudioBuffer.h"
#include "Audio/Core/AudioParameter.h"
#include "Audio/DSP/BasicProcessors.h"
#include "Audio/DSP/Pitch.h"
#include "Curve/Curve.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace Engine::Audio {

inline constexpr std::size_t kElectronicMistBandCount = 8;
inline constexpr std::size_t kElectronicMistCurveLutSize = 1024;
inline constexpr float kElectronicMistMinimumFrequencyHz = 20.0f;
inline constexpr float kElectronicMistMaximumFrequencyHz = 20'000.0f;

enum class ElectronicMistNoiseType : std::uint8_t {
    White,
    Soft,
    Digital
};

struct ElectronicMistConfig {
    bool enabled = false;
    float amount = 0.0f;
    float follow = 0.75f;
    float sharpness = 0.40f;
    float width = 0.40f;
    float wet = 1.0f;
    ElectronicMistNoiseType noiseType = ElectronicMistNoiseType::Soft;
};

[[nodiscard]] ElectronicMistConfig ClampElectronicMistConfig(
    ElectronicMistConfig config) noexcept;

struct ElectronicMistSnapshot {
    ElectronicMistConfig config{};
    std::array<float, kElectronicMistCurveLutSize> curve{};
    std::size_t curveSize = 0;
    std::uint64_t revision = 0;

    [[nodiscard]] float SampleCurve(float frequencyHz) const noexcept;
};

[[nodiscard]] std::shared_ptr<const ElectronicMistSnapshot> BuildElectronicMistSnapshot(
    ElectronicMistConfig config,
    std::shared_ptr<const Curve::CurveSnapshot> curve,
    std::uint64_t revision);

[[nodiscard]] std::shared_ptr<const ElectronicMistSnapshot> BuildElectronicMistSnapshot(
    ElectronicMistConfig config,
    std::uint64_t revision);

class ElectronicMistNoiseGenerator final {
public:
    explicit ElectronicMistNoiseGenerator(std::uint32_t seed = 0x13579BDFu) noexcept
        : m_Seed(seed == 0 ? 0x13579BDFu : seed), m_State(m_Seed) {}

    void Prepare(std::size_t channels) noexcept;
    void Reset() noexcept;
    [[nodiscard]] float Next(std::size_t channel, ElectronicMistNoiseType type) noexcept;
    [[nodiscard]] std::uint32_t Seed() const noexcept { return m_Seed; }

private:
    [[nodiscard]] float NextUnit() noexcept;

    std::uint32_t m_Seed = 0x13579BDFu;
    std::uint32_t m_State = 0x13579BDFu;
    std::array<float, 8> m_Previous{};
    std::array<float, 8> m_DigitalPrevious{};
    std::size_t m_Channels = 0;
};

class ElectronicMistVoiceBandFollower final {
public:
    AudioStatus Prepare(const AudioFormat& format) noexcept;
    void Reset() noexcept;
    void ProcessSample(
        ConstAudioBufferView input,
        std::size_t frame,
        bool voiced,
        float confidence) noexcept;

    [[nodiscard]] float BroadbandEnvelope() const noexcept { return m_BroadbandEnvelope; }
    [[nodiscard]] float GateActivity() const noexcept { return m_GateActivity; }
    [[nodiscard]] float Envelope(std::size_t band) const noexcept {
        return band < kElectronicMistBandCount ? m_Bands[band].envelope : 0.0f;
    }
    [[nodiscard]] float CenterFrequency(std::size_t band) const noexcept {
        return band < kElectronicMistBandCount ? m_Bands[band].centerHz : 0.0f;
    }
    [[nodiscard]] std::size_t BandCount() const noexcept { return kElectronicMistBandCount; }

private:
    struct BandState {
        float centerHz = 0.0f;
        float b0 = 0.0f;
        float b1 = 0.0f;
        float b2 = 0.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;
        float z1 = 0.0f;
        float z2 = 0.0f;
        float envelope = 0.0f;
    };

    AudioFormat m_Format{};
    std::array<BandState, kElectronicMistBandCount> m_Bands{};
    float m_AttackCoefficient = 0.0f;
    float m_ReleaseCoefficient = 0.0f;
    float m_BroadbandEnvelope = 0.0f;
    float m_GateActivity = 0.0f;
    bool m_Prepared = false;
};

struct ElectronicMistMetrics {
    bool enabled = false;
    bool bypassed = false;
    float amount = 0.0f;
    float follow = 0.75f;
    float sharpness = 0.40f;
    float width = 0.40f;
    float wet = 1.0f;
    float broadbandVoiceEnvelope = 0.0f;
    float voiceGateActivity = 0.0f;
    std::size_t bandCount = kElectronicMistBandCount;
    std::size_t activeBandCount = 0;
    float averageNoiseGain = 0.0f;
    float peakNoiseGain = 0.0f;
    float mistRms = 0.0f;
    float voiceRms = 0.0f;
    float mistToVoiceDb = -100.0f;
    std::uint64_t snapshotRevision = 0;
    std::uint64_t processedFrames = 0;
    std::uint32_t algorithmicLatencyFrames = 0;
    double processMilliseconds = 0.0;
    double maximumProcessMilliseconds = 0.0;
};

class ElectronicMistProcessor final : public ProcessorBase {
public:
    explicit ElectronicMistProcessor(std::uint32_t seed = 0x13579BDFu) noexcept
        : m_Noise(seed) {}

    AudioStatus Prepare(const AudioFormat& format) override;
    void Process(AudioProcessContext& context) noexcept override;
    void Reset() noexcept override;
    [[nodiscard]] const char* Name() const noexcept override { return "ElectronicMist"; }

    AudioStatus Publish(std::shared_ptr<const ElectronicMistSnapshot> snapshot);
    void SetVoiceEstimate(PitchEstimate estimate) noexcept {
        m_Voiced = estimate.voiced;
        m_Confidence = estimate.confidence;
    }
    [[nodiscard]] ElectronicMistMetrics CaptureMetrics() const noexcept;
    [[nodiscard]] std::uint32_t LatencyFrames() const noexcept { return 0; }

private:
    void AdoptSnapshot(const ElectronicMistSnapshot* snapshot) noexcept;

    AudioFormat m_Format{};
    AudioBuffer m_VoiceBuffer;
    ElectronicMistVoiceBandFollower m_Follower;
    ElectronicMistNoiseGenerator m_Noise;
    std::array<ParameterSmoother, 5> m_Smoothers{};
    std::vector<std::shared_ptr<const ElectronicMistSnapshot>> m_Retained;
    std::atomic<const ElectronicMistSnapshot*> m_Current{nullptr};
    const ElectronicMistSnapshot* m_AudioSnapshot = nullptr;
    ElectronicMistConfig m_Config{};
    bool m_Voiced = false;
    float m_Confidence = 0.0f;
    bool m_Prepared = false;
    std::atomic<bool> m_Enabled{false};
    std::atomic<float> m_Amount{0.0f};
    std::atomic<float> m_Follow{0.75f};
    std::atomic<float> m_Sharpness{0.40f};
    std::atomic<float> m_Width{0.40f};
    std::atomic<float> m_Wet{1.0f};
    std::atomic<float> m_BroadbandEnvelope{0.0f};
    std::atomic<float> m_GateActivity{0.0f};
    std::atomic<std::size_t> m_ActiveBandCount{0};
    std::atomic<float> m_AverageNoiseGain{0.0f};
    std::atomic<float> m_PeakNoiseGain{0.0f};
    std::atomic<float> m_MistRms{0.0f};
    std::atomic<float> m_VoiceRms{0.0f};
    std::atomic<float> m_MistToVoiceDb{-100.0f};
    std::atomic<std::uint64_t> m_Revision{0};
    std::atomic<std::uint64_t> m_ProcessedFrames{0};
    std::atomic<double> m_ProcessMilliseconds{0.0};
    std::atomic<double> m_MaximumProcessMilliseconds{0.0};
};

} // namespace Engine::Audio
