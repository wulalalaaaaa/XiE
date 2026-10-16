#pragma once

#include "Audio/Core/AudioTypes.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace Engine::Audio {

struct AudioMetricsSnapshot {
    std::uint64_t callbackCount = 0;
    std::uint64_t xRuns = 0;
    std::uint64_t droppedFrames = 0;
    double callbackMilliseconds = 0.0;
    double maxCallbackMilliseconds = 0.0;
    double dspMilliseconds = 0.0;
    double graphMilliseconds = 0.0;
    float detectedF0 = 0.0f;
    float pitchConfidence = 0.0f;
    AudioLatency latency{};
};

class AudioDiagnostics {
public:
    void RecordCallback(double milliseconds) noexcept;
    void RecordXRun(std::uint64_t droppedFrames = 0) noexcept;
    void RecordDspTime(double milliseconds) noexcept;
    void RecordGraphTime(double milliseconds) noexcept;
    void RecordPitch(float frequencyHz, float confidence) noexcept;
    void SetLatency(AudioLatency latency) noexcept;

    [[nodiscard]] AudioMetricsSnapshot Capture() const noexcept;

private:
    std::atomic<std::uint64_t> m_CallbackCount{0};
    std::atomic<std::uint64_t> m_XRuns{0};
    std::atomic<std::uint64_t> m_DroppedFrames{0};
    std::atomic<double> m_CallbackMilliseconds{0.0};
    std::atomic<double> m_MaxCallbackMilliseconds{0.0};
    std::atomic<double> m_DspMilliseconds{0.0};
    std::atomic<double> m_GraphMilliseconds{0.0};
    std::atomic<float> m_DetectedF0{0.0f};
    std::atomic<float> m_PitchConfidence{0.0f};
    std::atomic<double> m_CaptureLatency{0.0};
    std::atomic<double> m_ProcessingLatency{0.0};
    std::atomic<double> m_OutputLatency{0.0};
};

[[nodiscard]] std::string DumpAudioState(
    const AudioFormat& format,
    const AudioMetricsSnapshot& metrics);
[[nodiscard]] std::wstring DumpAudioDevices(const std::vector<AudioDeviceInfo>& devices);

} // namespace Engine::Audio
