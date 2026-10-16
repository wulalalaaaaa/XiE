#pragma once

#include "Audio/Diagnostics/AudioDiagnostics.h"
#include "Audio/Analysis/SpectrumAnalyzer.h"
#include "Audio/DSP/EQ/ParametricEQ.h"
#include "Audio/DSP/ElectronicMist/ElectronicMist.h"
#include "Audio/DSP/Formant.h"
#include "Audio/DSP/Haze/SoftHaze.h"
#include "Audio/DSP/Haze/SpatialHaze.h"
#include "Audio/DSP/Noise/NoiseSuppressorProcessor.h"
#include "Audio/DSP/Pitch.h"
#include "Audio/DSP/Synthetic/SyntheticVoice.h"

#include <algorithm>
#include <atomic>
#include <memory>
#include <string>

namespace Engine::Audio {

struct VoiceProcessorConfig {
    AudioFormat format{};
    PitchDetector::Config pitchDetector{};
    std::uint32_t pitchShiftWindowFrames = 512;
    float minimumPitchConfidence = 0.65f;
    float lowConfidenceHoldMilliseconds = 80.0f;
    float fallbackPitchSemitones = 0.0f;
    float fallbackFormantSemitones = 0.0f;
    ParametricEQModel parametricEQ{};
    SoftHazeConfig softHaze{};
    SpatialHazeConfig spatialHaze{};
    SyntheticVoiceConfig syntheticVoice{};
    ElectronicMistConfig electronicMist{};
    NoiseSuppressorConfig noiseSuppression{};
    SpectrumAnalyzerConfig analyzer{};
};

struct VoiceCurveSet {
    std::shared_ptr<const Curve::CurveSnapshot> pitch;
    std::shared_ptr<const Curve::CurveSnapshot> formant;
    std::shared_ptr<const Curve::CurveSnapshot> electronicNoise;
};

struct VoiceRealtimeState {
    PitchEstimate pitch{};
    float targetPitchSemitones = 0.0f;
    float targetFormantSemitones = 0.0f;
    float wet = 1.0f;
    bool bypassed = false;
    std::uint64_t parameterGeneration = 0;
    bool eqEnabled = true;
    bool noiseEnabled = false;
    bool softHazeEnabled = false;
    bool spatialHazeEnabled = false;
    bool syntheticVoiceEnabled = false;
    bool electronicMistEnabled = false;
};

struct VoiceProcessorMetrics {
    VoiceRealtimeState state{};
    float outputPeak = 0.0f;
    float outputRms = 0.0f;
    std::uint64_t processedFrames = 0;
    std::uint32_t algorithmLatencyFrames = 0;
    ParametricEQMetrics eq{};
    NoiseSuppressorMetrics noise{};
    SoftHazeMetrics softHaze{};
    SpatialHazeMetrics spatialHaze{};
    SyntheticVoiceMetrics syntheticVoice{};
    ElectronicMistMetrics electronicMist{};
    SpectrumAnalyzerMetrics analyzer{};
};

class VoiceProcessor final : public ProcessorBase {
public:
    VoiceProcessor();
    explicit VoiceProcessor(VoiceProcessorConfig config);

    AudioStatus Prepare(const AudioFormat& format) override;
    AudioStatus Prepare(const VoiceProcessorConfig& config);
    void Process(AudioProcessContext& context) noexcept override;
    void Reset() noexcept override;
    [[nodiscard]] const char* Name() const noexcept override { return "VoiceProcessor"; }

    void SetParameter(AudioParameterId id, float value) noexcept;
    AudioStatus PublishCurves(const VoiceCurveSet& curves);
    AudioStatus PublishParametricEQ(std::shared_ptr<const ParametricEQSnapshot> snapshot);
    AudioStatus PublishNoiseSuppression(
        std::shared_ptr<const NoiseSuppressorSnapshot> snapshot);
    AudioStatus PublishSoftHaze(std::shared_ptr<const SoftHazeSnapshot> snapshot);
    AudioStatus PublishSpatialHaze(std::shared_ptr<const SpatialHazeSnapshot> snapshot);
    AudioStatus PublishSyntheticVoice(std::shared_ptr<const SyntheticVoiceSnapshot> snapshot);
    AudioStatus PublishElectronicMist(std::shared_ptr<const ElectronicMistSnapshot> snapshot);
    void SetNoiseBypassed(bool bypassed) noexcept { m_NoiseSuppressor.SetBypassed(bypassed); }
    void SetEQBypassed(bool bypassed) noexcept { m_ParametricEQ.SetBypassed(bypassed); }
    void SetAnalyzerEnabled(bool enabled) noexcept { m_Analyzer.SetEnabled(enabled); }
    [[nodiscard]] std::shared_ptr<const SpectrumSnapshot> CaptureSpectrum() const noexcept {
        return m_Analyzer.CaptureSnapshot();
    }
    void SetDcBlockerBypassed(bool bypassed) noexcept { m_DcBlocker.SetBypassed(bypassed); }
    void SetPitchDetectorBypassed(bool bypassed) noexcept {
        m_PitchDetectorBypassed.store(bypassed, std::memory_order_release);
    }
    void SetPitchShiftBypassed(bool bypassed) noexcept { m_PitchShifter.SetBypassed(bypassed); }
    void SetFormantBypassed(bool bypassed) noexcept { m_FormantShifter.SetBypassed(bypassed); }
    void SetSoftHazeBypassed(bool bypassed) noexcept { m_SoftHaze.SetBypassed(bypassed); }
    void SetSpatialHazeBypassed(bool bypassed) noexcept { m_SpatialHaze.SetBypassed(bypassed); }
    void SetSyntheticVoiceBypassed(bool bypassed) noexcept { m_SyntheticVoice.SetBypassed(bypassed); }
    void SetElectronicMistBypassed(bool bypassed) noexcept { m_ElectronicMist.SetBypassed(bypassed); }
    void SetLimiterBypassed(bool bypassed) noexcept { m_Limiter.SetBypassed(bypassed); }

    [[nodiscard]] VoiceProcessorMetrics CaptureMetrics() const noexcept;
    [[nodiscard]] std::uint32_t LatencyFrames() const noexcept {
        const std::uint32_t base = m_NoiseSuppressor.LatencyFrames() +
            (m_PitchShifter.Bypassed() ? 0 : m_PitchShifter.LatencyFrames());
        // Micro pitch is a parallel layer. Report the longest audible path,
        // rather than adding its latency to the already delayed main path.
        return std::max(base, m_SyntheticVoice.LatencyFrames());
    }

private:
    VoiceProcessorConfig m_Config{};
    AudioParameterStore m_Parameters;
    AudioBuffer m_DryBuffer;
    DCBlocker m_DcBlocker;
    NoiseSuppressorProcessor m_NoiseSuppressor;
    PitchDetector m_PitchDetector;
    PitchCurve m_PitchCurve;
    PitchShiftProcessor m_PitchShifter;
    FormantCurve m_FormantCurve;
    FormantShiftProcessor m_FormantShifter;
    SyntheticVoiceProcessor m_SyntheticVoice;
    ParametricEQProcessor m_ParametricEQ;
    ElectronicMistProcessor m_ElectronicMist;
    SoftHazeProcessor m_SoftHaze;
    SpatialHazeProcessor m_SpatialHaze;
    Limiter m_Limiter;
    MeterProcessor m_Meter;
    SpectrumAnalyzer m_Analyzer;
    ParameterSmoother m_InputGainSmoother;
    ParameterSmoother m_WetSmoother;
    ParameterSmoother m_OutputGainSmoother;
    std::atomic<bool> m_PitchDetectorBypassed{false};
    std::atomic<float> m_DetectedFrequency{0.0f};
    std::atomic<float> m_PitchConfidence{0.0f};
    std::atomic<bool> m_PitchVoiced{false};
    std::atomic<float> m_TargetPitch{0.0f};
    std::atomic<float> m_TargetFormant{0.0f};
    std::atomic<float> m_CurrentWet{1.0f};
    std::atomic<bool> m_GlobalBypass{false};
    std::atomic<std::uint64_t> m_ProcessedFrames{0};
    std::atomic<std::uint64_t> m_LastParameterGeneration{0};
    float m_HeldPitchSemitones = 0.0f;
    float m_HeldFormantSemitones = 0.0f;
    std::uint32_t m_LowConfidenceFrames = 0;
    std::uint32_t m_HoldFrames = 0;
    bool m_Prepared = false;
};

[[nodiscard]] std::string DumpVoiceState(const VoiceProcessorMetrics& metrics);

} // namespace Engine::Audio
