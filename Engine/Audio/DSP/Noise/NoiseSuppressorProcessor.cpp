#include "Audio/DSP/Noise/NoiseSuppressorProcessor.h"

#include "Audio/DSP/Noise/RNNoiseSuppressor.h"

#include <algorithm>
#include <cmath>

namespace Engine::Audio {
namespace {
constexpr std::size_t kMaximumNoiseChannels = 8;
constexpr double kHistogramBucketMilliseconds = 0.05;
}

AudioStatus NoiseSuppressorProcessor::Prepare(const AudioFormat& format) {
    if (!format.IsValid() || format.sampleFormat != AudioSampleFormat::Float32 ||
        format.channelCount > kMaximumNoiseChannels) {
        return {AudioStatusCode::UnsupportedFormat, 0};
    }
    m_Format = format;
    m_MonoInput.assign(format.framesPerBuffer, 0.0f);
    m_DelayedDry.assign(format.framesPerBuffer, 0.0f);
    m_Processed.assign(format.framesPerBuffer, 0.0f);
    m_Algorithm = std::make_unique<RNNoiseSuppressor>();
    const AudioStatus algorithmStatus = m_Algorithm->Prepare({format});
    m_Available = static_cast<bool>(algorithmStatus);
    if (m_Available) {
        const AudioStatus adapterStatus = m_Adapter.Prepare(m_Algorithm->RequiredFrameSize());
        if (!adapterStatus) {
            m_Available = false;
            m_State.store(NoiseSuppressorState::InitializationFailed, std::memory_order_release);
        }
    } else {
        m_State.store(algorithmStatus.code == AudioStatusCode::UnsupportedFormat
            ? NoiseSuppressorState::UnsupportedFormat
            : NoiseSuppressorState::InitializationFailed, std::memory_order_release);
    }
    m_WetSmoother.Prepare(format.sampleRate, 10.0, 0.0f);
    m_Prepared = true;
    Reset();
    if (!m_Available) {
        m_State.store(format.sampleRate == 48'000
            ? NoiseSuppressorState::InitializationFailed
            : NoiseSuppressorState::UnsupportedFormat, std::memory_order_release);
    }
    // An unavailable denoiser is an explicit pass-through state, not a graph failure.
    return {};
}

AudioStatus NoiseSuppressorProcessor::Publish(
    std::shared_ptr<const NoiseSuppressorSnapshot> snapshot) {
    if (!snapshot) return {AudioStatusCode::InvalidArgument, 0};
    const NoiseSuppressorSnapshot* raw = snapshot.get();
    m_Enabled.store(raw->config.enabled, std::memory_order_release);
    m_Strength.store(raw->config.strength, std::memory_order_release);
    m_Wet.store(raw->config.wet, std::memory_order_release);
    m_Revision.store(raw->revision, std::memory_order_release);
    m_Retained.push_back(std::move(snapshot));
    m_Current.store(raw, std::memory_order_release);
    return {};
}

void NoiseSuppressorProcessor::AdoptSnapshot(
    const NoiseSuppressorSnapshot* snapshot) noexcept {
    m_AudioSnapshot = snapshot;
    const NoiseSuppressorConfig next = snapshot == nullptr
        ? NoiseSuppressorConfig{}
        : ClampNoiseSuppressorConfig(snapshot->config);
    if (next.enabled && !m_PreviousEnabled) m_ResetOnEnable = true;
    m_PreviousEnabled = next.enabled;
    m_AudioConfig = next;
    m_Enabled.store(next.enabled, std::memory_order_relaxed);
    m_Strength.store(next.strength, std::memory_order_relaxed);
    m_Wet.store(next.wet, std::memory_order_relaxed);
    m_Revision.store(snapshot == nullptr ? 0 : snapshot->revision, std::memory_order_relaxed);
}

void NoiseSuppressorProcessor::ResetStreamState() noexcept {
    m_Adapter.Reset();
    if (m_Algorithm) m_Algorithm->Reset();
    m_WetSmoother.Reset(0.0f);
    m_BufferedInputFrames.store(0, std::memory_order_relaxed);
    m_BufferedOutputFrames.store(0, std::memory_order_relaxed);
    m_ResetOnEnable = false;
}

void NoiseSuppressorProcessor::RecordAlgorithmFrame(
    const NoiseFrameResult& result) noexcept {
    const std::uint64_t frameCount = std::max<std::uint32_t>(1, result.algorithmFrames);
    if (!result.valid) {
        m_DroppedFrames.fetch_add(frameCount, std::memory_order_relaxed);
        return;
    }
    m_VoiceProbability.store(result.voiceProbability, std::memory_order_relaxed);
    m_AttenuationSumDb.fetch_add(
        result.attenuationDb * static_cast<float>(frameCount), std::memory_order_relaxed);
    float peak = m_PeakAttenuationDb.load(std::memory_order_relaxed);
    while (result.attenuationDb > peak && !m_PeakAttenuationDb.compare_exchange_weak(
        peak, result.attenuationDb, std::memory_order_relaxed)) {}
    m_ProcessSumMilliseconds.fetch_add(result.processMilliseconds, std::memory_order_relaxed);
    const double maximumFrameMilliseconds = result.maximumProcessMilliseconds > 0.0
        ? result.maximumProcessMilliseconds
        : result.processMilliseconds / static_cast<double>(frameCount);
    double maximum = m_MaximumProcessMilliseconds.load(std::memory_order_relaxed);
    while (maximumFrameMilliseconds > maximum &&
        !m_MaximumProcessMilliseconds.compare_exchange_weak(
            maximum, maximumFrameMilliseconds, std::memory_order_relaxed)) {}
    const double averageFrameMilliseconds =
        result.processMilliseconds / static_cast<double>(frameCount);
    const std::size_t bucket = std::min<std::size_t>(m_ProcessHistogram.size() - 1,
        static_cast<std::size_t>(averageFrameMilliseconds / kHistogramBucketMilliseconds));
    m_ProcessHistogram[bucket].fetch_add(frameCount, std::memory_order_relaxed);
    m_AlgorithmFrames.fetch_add(frameCount, std::memory_order_relaxed);
}

void NoiseSuppressorProcessor::Process(AudioProcessContext& context) noexcept {
    if (!m_Prepared) return;
    const NoiseSuppressorSnapshot* published = m_Current.load(std::memory_order_acquire);
    if (published != m_AudioSnapshot) AdoptSnapshot(published);

    const bool requested = m_AudioConfig.enabled &&
        m_AudioConfig.mode == NoiseSuppressorMode::RNNoise;
    if (!requested) {
        m_WetSmoother.SetTarget(0.0f);
        m_State.store(NoiseSuppressorState::Disabled, std::memory_order_relaxed);
        m_Active.store(false, std::memory_order_relaxed);
        return;
    }
    if (!m_Available || !m_Algorithm) {
        m_State.store(m_Format.sampleRate == 48'000
            ? NoiseSuppressorState::InitializationFailed
            : NoiseSuppressorState::UnsupportedFormat, std::memory_order_relaxed);
        m_Active.store(false, std::memory_order_relaxed);
        return;
    }
    if (m_ResetOnEnable) ResetStreamState();

    const std::size_t frames = std::min(context.output.FrameCount(), m_MonoInput.size());
    const std::size_t channels = context.output.ChannelCount();
    for (std::size_t frame = 0; frame < frames; ++frame) {
        double mono = 0.0;
        for (std::size_t channel = 0; channel < channels; ++channel) {
            const float sample = context.output.Sample(channel, frame);
            mono += std::isfinite(sample) ? sample : 0.0f;
        }
        m_MonoInput[frame] = static_cast<float>(mono / std::max<std::size_t>(1, channels));
    }
    const auto before = m_Adapter.CaptureMetrics();
    const NoiseFrameResult result = m_Adapter.Process(
        std::span<const float>(m_MonoInput.data(), frames),
        std::span<float>(m_DelayedDry.data(), frames),
        std::span<float>(m_Processed.data(), frames), *m_Algorithm);
    const auto after = m_Adapter.CaptureMetrics();
    if (after.algorithmFrames > before.algorithmFrames) RecordAlgorithmFrame(result);
    m_BufferedInputFrames.store(after.bufferedInputFrames, std::memory_order_relaxed);
    m_BufferedOutputFrames.store(after.bufferedOutputFrames, std::memory_order_relaxed);

    const bool bypassed = Bypassed();
    bool anyActive = false;
    const float suppressionStrength = 0.25f + 0.75f * m_AudioConfig.strength;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const bool processedReady = before.inputFrames + frame >= m_Adapter.LatencyFrames();
        const float targetWet = !bypassed && processedReady ? m_AudioConfig.wet : 0.0f;
        m_WetSmoother.SetTarget(targetWet);
        const float wet = m_WetSmoother.Next();
        const float strengthOutput = std::lerp(
            m_DelayedDry[frame], m_Processed[frame], suppressionStrength);
        for (std::size_t channel = 0; channel < channels; ++channel) {
            const float dry = std::isfinite(context.output.Sample(channel, frame))
                ? context.output.Sample(channel, frame) : 0.0f;
            context.output.Sample(channel, frame) = std::lerp(dry, strengthOutput, wet);
        }
        anyActive = anyActive || wet > 1.0e-4f;
    }
    m_ProcessedFrames.fetch_add(frames, std::memory_order_relaxed);
    m_Active.store(anyActive, std::memory_order_relaxed);
    m_State.store(bypassed ? NoiseSuppressorState::Bypassed
                           : NoiseSuppressorState::Active,
        std::memory_order_relaxed);
}

void NoiseSuppressorProcessor::Reset() noexcept {
    ResetStreamState();
    m_AudioSnapshot = nullptr;
    m_AudioConfig = {};
    m_PreviousEnabled = false;
    m_State.store(NoiseSuppressorState::Disabled, std::memory_order_release);
    m_Active.store(false, std::memory_order_release);
    m_VoiceProbability.store(0.0f, std::memory_order_release);
    m_AttenuationSumDb.store(0.0f, std::memory_order_release);
    m_PeakAttenuationDb.store(0.0f, std::memory_order_release);
    m_ProcessedFrames.store(0, std::memory_order_release);
    m_AlgorithmFrames.store(0, std::memory_order_release);
    m_DroppedFrames.store(0, std::memory_order_release);
    m_ProcessSumMilliseconds.store(0.0, std::memory_order_release);
    m_MaximumProcessMilliseconds.store(0.0, std::memory_order_release);
    for (auto& bucket : m_ProcessHistogram) bucket.store(0, std::memory_order_release);
}

double NoiseSuppressorProcessor::HistogramPercentile(double percentile) const noexcept {
    const std::uint64_t total = m_AlgorithmFrames.load(std::memory_order_acquire);
    if (total == 0) return 0.0;
    const std::uint64_t target = static_cast<std::uint64_t>(
        std::ceil(std::clamp(percentile, 0.0, 1.0) * total));
    std::uint64_t cumulative = 0;
    for (std::size_t index = 0; index < m_ProcessHistogram.size(); ++index) {
        cumulative += m_ProcessHistogram[index].load(std::memory_order_acquire);
        if (cumulative >= target) return (index + 1) * kHistogramBucketMilliseconds;
    }
    return m_ProcessHistogram.size() * kHistogramBucketMilliseconds;
}

NoiseSuppressorMetrics NoiseSuppressorProcessor::CaptureMetrics() const noexcept {
    NoiseSuppressorMetrics metrics;
    metrics.state = m_State.load(std::memory_order_acquire);
    metrics.mode = NoiseSuppressorMode::RNNoise;
    metrics.enabled = m_Enabled.load(std::memory_order_acquire);
    metrics.bypassed = Bypassed();
    metrics.active = m_Active.load(std::memory_order_acquire);
    metrics.strength = m_Strength.load(std::memory_order_acquire);
    metrics.wet = m_Wet.load(std::memory_order_acquire);
    metrics.voiceProbability = m_VoiceProbability.load(std::memory_order_acquire);
    metrics.processedFrames = m_ProcessedFrames.load(std::memory_order_acquire);
    metrics.algorithmFrames = m_AlgorithmFrames.load(std::memory_order_acquire);
    metrics.droppedFrames = m_DroppedFrames.load(std::memory_order_acquire);
    metrics.snapshotRevision = m_Revision.load(std::memory_order_acquire);
    const float attenuation = m_AttenuationSumDb.load(std::memory_order_acquire);
    metrics.averageAttenuationDb = metrics.algorithmFrames > 0
        ? attenuation / static_cast<float>(metrics.algorithmFrames) : 0.0f;
    metrics.peakAttenuationDb = m_PeakAttenuationDb.load(std::memory_order_acquire);
    const double sum = m_ProcessSumMilliseconds.load(std::memory_order_acquire);
    metrics.averageProcessMilliseconds = metrics.algorithmFrames > 0
        ? sum / static_cast<double>(metrics.algorithmFrames) : 0.0;
    metrics.p95ProcessMilliseconds = HistogramPercentile(0.95);
    metrics.p99ProcessMilliseconds = HistogramPercentile(0.99);
    metrics.maximumProcessMilliseconds =
        m_MaximumProcessMilliseconds.load(std::memory_order_acquire);
    metrics.frameSize = m_Algorithm
        ? static_cast<std::uint32_t>(m_Algorithm->RequiredFrameSize()) : 0;
    metrics.bufferedInputFrames = m_BufferedInputFrames.load(std::memory_order_acquire);
    metrics.bufferedOutputFrames = m_BufferedOutputFrames.load(std::memory_order_acquire);
    metrics.algorithmicLatencyMilliseconds = metrics.enabled && m_Available &&
        m_Format.sampleRate > 0
        ? 1000.0 * metrics.frameSize / m_Format.sampleRate : 0.0;
    return metrics;
}

std::uint32_t NoiseSuppressorProcessor::LatencyFrames() const noexcept {
    return m_Available && m_Enabled.load(std::memory_order_acquire) && m_Algorithm
        ? static_cast<std::uint32_t>(m_Algorithm->RequiredFrameSize()) : 0;
}

} // namespace Engine::Audio
