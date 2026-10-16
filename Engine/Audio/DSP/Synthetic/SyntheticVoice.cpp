#include "Audio/DSP/Synthetic/SyntheticVoice.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace Engine::Audio {

SyntheticVoiceConfig ClampSyntheticVoiceConfig(
    SyntheticVoiceConfig config) noexcept {
    config.amount = std::clamp(
        std::isfinite(config.amount) ? config.amount : 0.0f, 0.0f, 1.0f);
    config.stability = std::clamp(
        std::isfinite(config.stability) ? config.stability : 0.0f, 0.0f, 1.0f);
    config.electronic = std::clamp(
        std::isfinite(config.electronic) ? config.electronic : 0.0f, 0.0f, 1.0f);
    config.width = std::clamp(
        std::isfinite(config.width) ? config.width : 0.5f, 0.0f, 1.0f);
    config.wet = std::clamp(
        std::isfinite(config.wet) ? config.wet : 1.0f, 0.0f, 1.0f);
    return config;
}

std::shared_ptr<const SyntheticVoiceSnapshot> BuildSyntheticVoiceSnapshot(
    SyntheticVoiceConfig config,
    std::uint64_t revision) {
    auto snapshot = std::make_shared<SyntheticVoiceSnapshot>();
    snapshot->config = ClampSyntheticVoiceConfig(config);
    snapshot->revision = revision;
    return snapshot;
}

AudioStatus SyntheticVoiceProcessor::Prepare(const AudioFormat& format) {
    if (!format.IsValid()) return {AudioStatusCode::InvalidArgument, 0};
    m_Format = format;
    const AudioStatus status = m_MicroPitch.Prepare(format);
    if (!status) return status;
    m_Stabilizer.Prepare(format);
    m_Prepared = true;
    Reset();
    return {};
}

void SyntheticVoiceProcessor::AdoptSnapshot(
    const SyntheticVoiceSnapshot* snapshot) noexcept {
    if (snapshot == nullptr || snapshot == m_AudioSnapshot) return;
    m_AudioSnapshot = snapshot;
    m_Config = ClampSyntheticVoiceConfig(snapshot->config);
    m_Stabilizer.SetConfig({
        m_Config.enabled && m_Config.amount > 1.0e-4f,
        m_Config.stability * m_Config.amount,
        0.65f,
        12.0f * m_Config.amount,
        0.65f,
        80.0f});
    m_MicroPitch.SetConfig({
        m_Config.enabled && m_Config.electronic > 1.0e-4f,
        m_Config.amount * m_Config.electronic,
        m_Config.electronic,
        m_Config.width,
        m_Config.wet},
        snapshot->revision);
    m_Enabled.store(m_Config.enabled, std::memory_order_relaxed);
    m_Amount.store(m_Config.amount, std::memory_order_relaxed);
    m_Stability.store(m_Config.stability, std::memory_order_relaxed);
    m_Electronic.store(m_Config.electronic, std::memory_order_relaxed);
    m_Width.store(m_Config.width, std::memory_order_relaxed);
    m_Wet.store(m_Config.wet, std::memory_order_relaxed);
    m_Revision.store(snapshot->revision, std::memory_order_relaxed);
}

AudioStatus SyntheticVoiceProcessor::Publish(
    std::shared_ptr<const SyntheticVoiceSnapshot> snapshot) {
    if (!snapshot) return {AudioStatusCode::InvalidArgument, 0};
    m_Retained.push_back(snapshot);
    m_Current.store(snapshot.get(), std::memory_order_release);
    return {};
}

PitchStabilizerResult SyntheticVoiceProcessor::UpdatePitch(
    PitchEstimate estimate,
    std::uint32_t frameCount) noexcept {
    if (!m_Prepared) return {};
    AdoptSnapshot(m_Current.load(std::memory_order_acquire));
    const PitchStabilizerResult result = m_Stabilizer.Process(estimate, frameCount);
    m_RawFrequencyHz.store(result.rawFrequencyHz, std::memory_order_relaxed);
    m_StabilizedFrequencyHz.store(result.stabilizedFrequencyHz, std::memory_order_relaxed);
    m_CorrectionCents.store(result.correctionCents, std::memory_order_relaxed);
    m_Confidence.store(result.confidence, std::memory_order_relaxed);
    m_Voiced.store(result.voiced, std::memory_order_relaxed);
    m_CorrectionHeld.store(result.held, std::memory_order_relaxed);
    return result;
}

void SyntheticVoiceProcessor::Process(AudioProcessContext& context) noexcept {
    if (!m_Prepared || context.output.Empty()) return;
    AdoptSnapshot(m_Current.load(std::memory_order_acquire));
    const auto begin = std::chrono::steady_clock::now();
    m_MicroPitch.Process(context);
    m_ProcessedFrames.fetch_add(context.output.FrameCount(), std::memory_order_relaxed);
    const double elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - begin).count();
    m_ProcessMilliseconds.store(elapsed, std::memory_order_relaxed);
    double previous = m_MaximumProcessMilliseconds.load(std::memory_order_relaxed);
    while (elapsed > previous && !m_MaximumProcessMilliseconds.compare_exchange_weak(
        previous, elapsed, std::memory_order_relaxed)) {}
}

void SyntheticVoiceProcessor::Reset() noexcept {
    m_Stabilizer.Reset();
    m_MicroPitch.Reset();
    m_RawFrequencyHz.store(0.0f, std::memory_order_release);
    m_StabilizedFrequencyHz.store(0.0f, std::memory_order_release);
    m_CorrectionCents.store(0.0f, std::memory_order_release);
    m_Confidence.store(0.0f, std::memory_order_release);
    m_Voiced.store(false, std::memory_order_release);
    m_CorrectionHeld.store(false, std::memory_order_release);
    m_ProcessedFrames.store(0, std::memory_order_release);
    m_ProcessMilliseconds.store(0.0, std::memory_order_release);
    m_MaximumProcessMilliseconds.store(0.0, std::memory_order_release);
}

std::uint32_t SyntheticVoiceProcessor::LatencyFrames() const noexcept {
    return m_MicroPitch.CaptureMetrics().enabled ? m_MicroPitch.LatencyFrames() : 0;
}

SyntheticVoiceMetrics SyntheticVoiceProcessor::CaptureMetrics() const noexcept {
    SyntheticVoiceMetrics metrics;
    metrics.enabled = m_Enabled.load(std::memory_order_acquire);
    metrics.bypassed = Bypassed();
    metrics.amount = m_Amount.load(std::memory_order_acquire);
    metrics.stability = m_Stability.load(std::memory_order_acquire);
    metrics.electronic = m_Electronic.load(std::memory_order_acquire);
    metrics.width = m_Width.load(std::memory_order_acquire);
    metrics.wet = m_Wet.load(std::memory_order_acquire);
    metrics.rawFrequencyHz = m_RawFrequencyHz.load(std::memory_order_acquire);
    metrics.stabilizedFrequencyHz = m_StabilizedFrequencyHz.load(std::memory_order_acquire);
    metrics.correctionCents = m_CorrectionCents.load(std::memory_order_acquire);
    metrics.confidence = m_Confidence.load(std::memory_order_acquire);
    metrics.voiced = m_Voiced.load(std::memory_order_acquire);
    metrics.correctionHeld = m_CorrectionHeld.load(std::memory_order_acquire);
    metrics.controlSmoothingMilliseconds = m_Stabilizer.ControlSmoothingMilliseconds();
    metrics.micro = m_MicroPitch.CaptureMetrics();
    metrics.snapshotRevision = m_Revision.load(std::memory_order_acquire);
    metrics.processedFrames = m_ProcessedFrames.load(std::memory_order_acquire);
    metrics.algorithmicLatencyFrames = LatencyFrames();
    metrics.processMilliseconds = m_ProcessMilliseconds.load(std::memory_order_acquire);
    metrics.maximumProcessMilliseconds =
        m_MaximumProcessMilliseconds.load(std::memory_order_acquire);
    return metrics;
}

} // namespace Engine::Audio
