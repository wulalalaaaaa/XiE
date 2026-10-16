#pragma once

#include "Audio/Core/AudioBuffer.h"
#include "Audio/Core/AudioTypes.h"

#include <atomic>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

namespace Engine::Audio {

struct SpectrumAnalyzerConfig {
    bool enabled = true;
    std::size_t fftSize = 2048;
    float updateRateHz = 30.0f;
    float minimumFrequencyHz = 20.0f;
    float maximumFrequencyHz = 20'000.0f;
    float minimumDecibels = -100.0f;
    float maximumDecibels = 0.0f;
    float attack = 0.62f;
    float release = 0.16f;
};

struct SpectrumSnapshot {
    std::shared_ptr<const std::vector<float>> frequencyBins;
    std::vector<float> inputMagnitudeDb;
    std::vector<float> outputMagnitudeDb;
    double timestampSeconds = 0.0;
    std::uint64_t revision = 0;
    float peakDb = -100.0f;
};

struct SpectrumAnalyzerMetrics {
    std::size_t fftSize = 0;
    float updateRateHz = 0.0f;
    double workerMilliseconds = 0.0;
    double maximumWorkerMilliseconds = 0.0;
    std::uint64_t droppedWindows = 0;
    std::uint64_t revision = 0;
};

class SpectrumAnalyzer {
public:
    SpectrumAnalyzer() = default;
    ~SpectrumAnalyzer();
    SpectrumAnalyzer(const SpectrumAnalyzer&) = delete;
    SpectrumAnalyzer& operator=(const SpectrumAnalyzer&) = delete;

    AudioStatus Prepare(const AudioFormat& format, SpectrumAnalyzerConfig config = {});
    void Reset() noexcept;
    void SetEnabled(bool enabled) noexcept { m_Enabled.store(enabled, std::memory_order_release); }
    [[nodiscard]] bool Enabled() const noexcept {
        return m_Enabled.load(std::memory_order_acquire);
    }
    void PushInput(ConstAudioBufferView input) noexcept;
    void PushOutput(ConstAudioBufferView output) noexcept;
    [[nodiscard]] std::shared_ptr<const SpectrumSnapshot> CaptureSnapshot() const noexcept;
    [[nodiscard]] SpectrumAnalyzerMetrics CaptureMetrics() const noexcept;

    [[nodiscard]] static std::vector<float> BuildHannWindow(std::size_t size);
    [[nodiscard]] static std::vector<float> AnalyzeWindow(
        const std::vector<float>& samples,
        const std::vector<float>& window,
        float minimumDecibels = -100.0f);
    [[nodiscard]] static float SmoothMagnitudeDb(
        float currentDb, float targetDb, float attack, float release) noexcept;

private:
    class SampleRing {
    public:
        void Prepare(std::size_t capacity);
        void Reset() noexcept;
        [[nodiscard]] std::size_t Push(ConstAudioBufferView input) noexcept;
        [[nodiscard]] std::size_t Pop(float* destination, std::size_t count) noexcept;
        [[nodiscard]] std::size_t Available() const noexcept;

    private:
        std::vector<float> m_Data;
        std::size_t m_Capacity = 0;
        alignas(64) std::atomic<std::size_t> m_Read{0};
        alignas(64) std::atomic<std::size_t> m_Write{0};
    };

    void StopWorker() noexcept;
    void WorkerLoop() noexcept;
    bool AdvanceWindows() noexcept;
    void PublishAnalysis() noexcept;
    static void Transform(std::vector<std::complex<float>>& values) noexcept;
    void Smooth(const std::vector<float>& source, std::vector<float>& destination) const noexcept;

    AudioFormat m_Format{};
    SpectrumAnalyzerConfig m_Config{};
    SampleRing m_InputRing;
    SampleRing m_OutputRing;
    std::vector<float> m_Hann;
    std::vector<float> m_InputWindow;
    std::vector<float> m_OutputWindow;
    std::vector<float> m_InputScratch;
    std::vector<float> m_OutputScratch;
    std::vector<float> m_SmoothedInput;
    std::vector<float> m_SmoothedOutput;
    std::vector<std::complex<float>> m_FftWork;
    std::shared_ptr<const std::vector<float>> m_FrequencyBins;
    std::atomic<std::shared_ptr<const SpectrumSnapshot>> m_Latest;
    std::thread m_Worker;
    std::atomic<bool> m_StopRequested{false};
    std::atomic<bool> m_Enabled{true};
    std::atomic<std::uint64_t> m_DroppedWindows{0};
    std::atomic<std::uint64_t> m_Revision{0};
    std::atomic<double> m_WorkerMilliseconds{0.0};
    std::atomic<double> m_MaximumWorkerMilliseconds{0.0};
    std::size_t m_HopSize = 0;
    std::atomic<bool> m_Primed{false};
};

} // namespace Engine::Audio
