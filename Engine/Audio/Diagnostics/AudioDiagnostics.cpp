#include "Audio/Diagnostics/AudioDiagnostics.h"

#include <sstream>

namespace Engine::Audio {

void AudioDiagnostics::RecordCallback(double milliseconds) noexcept {
    m_CallbackCount.fetch_add(1, std::memory_order_relaxed);
    m_CallbackMilliseconds.store(milliseconds, std::memory_order_relaxed);
    double maximum = m_MaxCallbackMilliseconds.load(std::memory_order_relaxed);
    while (milliseconds > maximum &&
        !m_MaxCallbackMilliseconds.compare_exchange_weak(
            maximum, milliseconds, std::memory_order_relaxed)) {
    }
}

void AudioDiagnostics::RecordXRun(std::uint64_t droppedFrames) noexcept {
    m_XRuns.fetch_add(1, std::memory_order_relaxed);
    m_DroppedFrames.fetch_add(droppedFrames, std::memory_order_relaxed);
}

void AudioDiagnostics::RecordDspTime(double milliseconds) noexcept {
    m_DspMilliseconds.store(milliseconds, std::memory_order_relaxed);
}

void AudioDiagnostics::RecordGraphTime(double milliseconds) noexcept {
    m_GraphMilliseconds.store(milliseconds, std::memory_order_relaxed);
}

void AudioDiagnostics::RecordPitch(float frequencyHz, float confidence) noexcept {
    m_DetectedF0.store(frequencyHz, std::memory_order_relaxed);
    m_PitchConfidence.store(confidence, std::memory_order_relaxed);
}

void AudioDiagnostics::SetLatency(AudioLatency latency) noexcept {
    m_CaptureLatency.store(latency.captureMilliseconds, std::memory_order_relaxed);
    m_ProcessingLatency.store(latency.processingMilliseconds, std::memory_order_relaxed);
    m_OutputLatency.store(latency.outputMilliseconds, std::memory_order_relaxed);
}

AudioMetricsSnapshot AudioDiagnostics::Capture() const noexcept {
    AudioMetricsSnapshot result;
    result.callbackCount = m_CallbackCount.load(std::memory_order_acquire);
    result.xRuns = m_XRuns.load(std::memory_order_acquire);
    result.droppedFrames = m_DroppedFrames.load(std::memory_order_acquire);
    result.callbackMilliseconds = m_CallbackMilliseconds.load(std::memory_order_acquire);
    result.maxCallbackMilliseconds = m_MaxCallbackMilliseconds.load(std::memory_order_acquire);
    result.dspMilliseconds = m_DspMilliseconds.load(std::memory_order_acquire);
    result.graphMilliseconds = m_GraphMilliseconds.load(std::memory_order_acquire);
    result.detectedF0 = m_DetectedF0.load(std::memory_order_acquire);
    result.pitchConfidence = m_PitchConfidence.load(std::memory_order_acquire);
    result.latency.captureMilliseconds = m_CaptureLatency.load(std::memory_order_acquire);
    result.latency.processingMilliseconds = m_ProcessingLatency.load(std::memory_order_acquire);
    result.latency.outputMilliseconds = m_OutputLatency.load(std::memory_order_acquire);
    return result;
}

std::string DumpAudioState(const AudioFormat& format, const AudioMetricsSnapshot& metrics) {
    std::ostringstream output;
    output << "SampleRate=" << format.sampleRate << '\n'
           << "Channels=" << format.channelCount << '\n'
           << "FramesPerBuffer=" << format.framesPerBuffer << '\n'
           << "Callbacks=" << metrics.callbackCount << '\n'
           << "CallbackMs=" << metrics.callbackMilliseconds << '\n'
           << "MaxCallbackMs=" << metrics.maxCallbackMilliseconds << '\n'
           << "XRuns=" << metrics.xRuns << '\n'
           << "DroppedFrames=" << metrics.droppedFrames << '\n'
           << "DetectedF0=" << metrics.detectedF0 << '\n'
           << "PitchConfidence=" << metrics.pitchConfidence << '\n'
           << "TotalEstimatedLatencyMs=" << metrics.latency.TotalEstimatedMilliseconds() << '\n';
    return output.str();
}

std::wstring DumpAudioDevices(const std::vector<AudioDeviceInfo>& devices) {
    std::wostringstream output;
    for (std::size_t i = 0; i < devices.size(); ++i) {
        output << i << L": " << devices[i].name
               << (devices[i].isDefault ? L" [default]" : L"")
               << L" (" << devices[i].mixFormat.sampleRate << L" Hz, "
               << devices[i].mixFormat.channelCount << L" ch)\n";
    }
    return output.str();
}

} // namespace Engine::Audio
