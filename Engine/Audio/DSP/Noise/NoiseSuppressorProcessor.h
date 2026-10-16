#pragma once

#include "Audio/DSP/BasicProcessors.h"
#include "Audio/DSP/Noise/NoiseFrameAdapter.h"

#include <array>
#include <atomic>
#include <memory>
#include <vector>

namespace Engine::Audio {

class NoiseSuppressorProcessor final : public ProcessorBase {
public:
    AudioStatus Prepare(const AudioFormat& format) override;
    void Process(AudioProcessContext& context) noexcept override;
    void Reset() noexcept override;
    [[nodiscard]] const char* Name() const noexcept override { return "NoiseSuppressor"; }

    AudioStatus Publish(std::shared_ptr<const NoiseSuppressorSnapshot> snapshot);
    [[nodiscard]] NoiseSuppressorMetrics CaptureMetrics() const noexcept;
    [[nodiscard]] std::uint32_t LatencyFrames() const noexcept;

private:
    void AdoptSnapshot(const NoiseSuppressorSnapshot* snapshot) noexcept;
    void ResetStreamState() noexcept;
    void RecordAlgorithmFrame(const NoiseFrameResult& result) noexcept;
    [[nodiscard]] double HistogramPercentile(double percentile) const noexcept;

    AudioFormat m_Format{};
    std::unique_ptr<INoiseSuppressor> m_Algorithm;
    NoiseFrameAdapter m_Adapter;
    std::vector<float> m_MonoInput;
    std::vector<float> m_DelayedDry;
    std::vector<float> m_Processed;
    ParameterSmoother m_WetSmoother;
    std::vector<std::shared_ptr<const NoiseSuppressorSnapshot>> m_Retained;
    std::atomic<const NoiseSuppressorSnapshot*> m_Current{nullptr};
    const NoiseSuppressorSnapshot* m_AudioSnapshot = nullptr;
    NoiseSuppressorConfig m_AudioConfig{};
    bool m_Available = false;
    bool m_Prepared = false;
    bool m_ResetOnEnable = false;
    bool m_PreviousEnabled = false;

    std::atomic<NoiseSuppressorState> m_State{NoiseSuppressorState::Disabled};
    std::atomic<bool> m_Enabled{false};
    std::atomic<bool> m_Active{false};
    std::atomic<float> m_Strength{1.0f};
    std::atomic<float> m_Wet{1.0f};
    std::atomic<float> m_VoiceProbability{0.0f};
    std::atomic<float> m_AttenuationSumDb{0.0f};
    std::atomic<float> m_PeakAttenuationDb{0.0f};
    std::atomic<std::uint64_t> m_ProcessedFrames{0};
    std::atomic<std::uint64_t> m_AlgorithmFrames{0};
    std::atomic<std::uint64_t> m_DroppedFrames{0};
    std::atomic<std::uint64_t> m_Revision{0};
    std::atomic<double> m_ProcessSumMilliseconds{0.0};
    std::atomic<double> m_MaximumProcessMilliseconds{0.0};
    std::array<std::atomic<std::uint64_t>, 64> m_ProcessHistogram{};
    std::atomic<std::uint32_t> m_BufferedInputFrames{0};
    std::atomic<std::uint32_t> m_BufferedOutputFrames{0};
};

} // namespace Engine::Audio
