#include "Audio/DSP/Haze/SoftHaze.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace Engine::Audio {

SoftHazeConfig ClampSoftHazeConfig(SoftHazeConfig config) noexcept {
    auto finite = [](float value, float fallback) {
        return std::isfinite(value) ? value : fallback;
    };
    config.amount = std::clamp(finite(config.amount, 0.0f), 0.0f, 1.0f);
    config.blur = std::clamp(finite(config.blur, 0.35f), 0.0f, 1.0f);
    config.damping = std::clamp(finite(config.damping, 0.35f), 0.0f, 1.0f);
    config.wet = std::clamp(finite(config.wet, 0.20f), 0.0f, 1.0f);
    return config;
}

std::shared_ptr<const SoftHazeSnapshot> BuildSoftHazeSnapshot(
    SoftHazeConfig config,
    std::uint64_t revision) {
    auto snapshot = std::make_shared<SoftHazeSnapshot>();
    snapshot->config = ClampSoftHazeConfig(config);
    snapshot->revision = revision;
    return snapshot;
}

AudioStatus SoftHazeProcessor::Prepare(const AudioFormat& format) {
    if (!format.IsValid()) return {AudioStatusCode::InvalidArgument, 0};
    m_Format = format;
    m_MaximumDelaySamples = std::max<std::uint32_t>(
        2, static_cast<std::uint32_t>(format.sampleRate * 0.020));
    AudioStatus status = m_Diffuser.Prepare(format, m_MaximumDelaySamples);
    if (!status) return status;
    status = m_DampingFilter.Prepare(format);
    if (!status) return status;
    m_EnableMix.Prepare(format.sampleRate, 8.0, 0.0f);
    m_Amount.Prepare(format.sampleRate, 8.0, 0.0f);
    m_Blur.Prepare(format.sampleRate, 8.0, 0.35f);
    m_Damping.Prepare(format.sampleRate, 8.0, 0.35f);
    m_Wet.Prepare(format.sampleRate, 8.0, 0.20f);
    m_Prepared = true;
    Reset();
    return {};
}

AudioStatus SoftHazeProcessor::Publish(std::shared_ptr<const SoftHazeSnapshot> snapshot) {
    if (!snapshot) return {AudioStatusCode::InvalidArgument, 0};
    const SoftHazeSnapshot* raw = snapshot.get();
    m_Retained.push_back(std::move(snapshot));
    m_Current.store(raw, std::memory_order_release);
    return {};
}

void SoftHazeProcessor::AdoptSnapshot(const SoftHazeSnapshot* snapshot) noexcept {
    m_AudioSnapshot = snapshot;
    const SoftHazeConfig config = snapshot == nullptr
        ? SoftHazeConfig{}
        : ClampSoftHazeConfig(snapshot->config);
    m_Enabled.store(config.enabled, std::memory_order_relaxed);
    m_Revision.store(snapshot == nullptr ? 0 : snapshot->revision, std::memory_order_relaxed);
    m_Amount.SetTarget(config.amount);
    m_Blur.SetTarget(config.blur);
    m_Damping.SetTarget(config.damping);
    m_Wet.SetTarget(config.wet);
}

void SoftHazeProcessor::Process(AudioProcessContext& context) noexcept {
    const auto begin = std::chrono::steady_clock::now();
    if (!m_Prepared || context.output.Empty()) return;
    const SoftHazeSnapshot* published = m_Current.load(std::memory_order_acquire);
    if (published != m_AudioSnapshot) AdoptSnapshot(published);
    const bool active = m_AudioSnapshot != nullptr && m_AudioSnapshot->config.enabled && !Bypassed();
    m_EnableMix.SetTarget(active ? 1.0f : 0.0f);

    for (std::size_t frame = 0; frame < context.output.FrameCount(); ++frame) {
        const float amount = m_Amount.Next();
        const float blur = m_Blur.Next();
        const float damping = m_Damping.Next();
        const float wet = m_Wet.Next();
        const float enableMix = m_EnableMix.Next();
        m_CurrentAmount.store(amount, std::memory_order_relaxed);
        m_CurrentBlur.store(blur, std::memory_order_relaxed);
        m_CurrentDamping.store(damping, std::memory_order_relaxed);
        m_CurrentWet.store(wet, std::memory_order_relaxed);
        const std::uint32_t delaySamples = std::clamp<std::uint32_t>(
            1u + static_cast<std::uint32_t>(blur *
                static_cast<float>(m_Format.sampleRate) * 0.010f),
            1u, m_MaximumDelaySamples);
        m_EffectiveDelaySamples.store(delaySamples, std::memory_order_relaxed);
        m_Diffuser.SetParameters(delaySamples, 0.05f + amount * 0.25f, damping);
        m_DampingFilter.SetDamping(damping * 0.55f);
        const float mix = std::clamp(enableMix * wet * amount, 0.0f, 1.0f);
        for (std::size_t channel = 0; channel < context.output.ChannelCount(); ++channel) {
            const float input = context.output.Sample(channel, frame);
            const float diffused = m_Diffuser.ProcessChannel(channel, input);
            const float damped = m_DampingFilter.ProcessSample(channel, diffused);
            const float processed = input * (1.0f - 0.35f * amount) + damped * (0.35f * amount);
            context.output.Sample(channel, frame) = std::lerp(input, processed, mix);
        }
        m_Diffuser.Advance();
    }
    const double elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - begin).count();
    m_ProcessMilliseconds.store(elapsed, std::memory_order_relaxed);
    double maximum = m_MaximumProcessMilliseconds.load(std::memory_order_relaxed);
    while (elapsed > maximum && !m_MaximumProcessMilliseconds.compare_exchange_weak(
        maximum, elapsed, std::memory_order_relaxed)) {}
    m_ProcessedFrames.fetch_add(context.output.FrameCount(), std::memory_order_relaxed);
}

void SoftHazeProcessor::Reset() noexcept {
    m_Diffuser.Reset();
    m_DampingFilter.Reset();
    m_AudioSnapshot = nullptr;
    m_EnableMix.Reset(0.0f);
    m_Amount.Reset(0.0f);
    m_Blur.Reset(0.35f);
    m_Damping.Reset(0.35f);
    m_Wet.Reset(0.20f);
    m_Enabled.store(false, std::memory_order_release);
    m_Revision.store(0, std::memory_order_release);
    m_ProcessedFrames.store(0, std::memory_order_release);
    m_EffectiveDelaySamples.store(0, std::memory_order_release);
    m_CurrentAmount.store(0.0f, std::memory_order_release);
    m_CurrentBlur.store(0.35f, std::memory_order_release);
    m_CurrentDamping.store(0.35f, std::memory_order_release);
    m_CurrentWet.store(0.20f, std::memory_order_release);
    m_ProcessMilliseconds.store(0.0, std::memory_order_release);
    m_MaximumProcessMilliseconds.store(0.0, std::memory_order_release);
}

SoftHazeMetrics SoftHazeProcessor::CaptureMetrics() const noexcept {
    const std::uint32_t delay = m_EffectiveDelaySamples.load(std::memory_order_acquire);
    return {m_Enabled.load(std::memory_order_acquire), Bypassed(),
        m_CurrentAmount.load(std::memory_order_acquire),
        m_CurrentBlur.load(std::memory_order_acquire),
        m_CurrentDamping.load(std::memory_order_acquire),
        m_CurrentWet.load(std::memory_order_acquire),
        m_Revision.load(std::memory_order_acquire),
        m_ProcessedFrames.load(std::memory_order_acquire), delay,
        m_Format.sampleRate > 0 ? 1000.0 * delay / m_Format.sampleRate : 0.0,
        0, m_Diffuser.MemoryBytes() + m_DampingFilter.MemoryBytes(),
        m_ProcessMilliseconds.load(std::memory_order_acquire),
        m_MaximumProcessMilliseconds.load(std::memory_order_acquire)};
}

} // namespace Engine::Audio
