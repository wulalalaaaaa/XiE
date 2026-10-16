#include "Audio/DSP/Haze/SpatialHaze.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>

namespace Engine::Audio {

SpatialHazeConfig ClampSpatialHazeConfig(SpatialHazeConfig config) noexcept {
    auto finite = [](float value, float fallback) {
        return std::isfinite(value) ? value : fallback;
    };
    config.amount = std::clamp(finite(config.amount, 0.0f), 0.0f, 1.0f);
    config.size = std::clamp(finite(config.size, 0.35f), 0.0f, 1.0f);
    config.diffusion = std::clamp(finite(config.diffusion, 0.50f), 0.0f, 1.0f);
    config.damping = std::clamp(finite(config.damping, 0.40f), 0.0f, 1.0f);
    config.width = std::clamp(finite(config.width, 0.70f), 0.0f, 1.0f);
    config.wet = std::clamp(finite(config.wet, 0.12f), 0.0f, 1.0f);
    return config;
}

std::shared_ptr<const SpatialHazeSnapshot> BuildSpatialHazeSnapshot(
    SpatialHazeConfig config,
    std::uint64_t revision) {
    auto snapshot = std::make_shared<SpatialHazeSnapshot>();
    snapshot->config = ClampSpatialHazeConfig(config);
    snapshot->revision = revision;
    return snapshot;
}

AudioStatus SpatialHazeProcessor::Prepare(const AudioFormat& format) {
    if (!format.IsValid()) return {AudioStatusCode::InvalidArgument, 0};
    m_Format = format;
    m_MaximumDelaySamples = std::max<std::uint32_t>(
        2, static_cast<std::uint32_t>(format.sampleRate * 0.160));
    AudioStatus status = m_Network.Prepare(format, m_MaximumDelaySamples);
    if (!status) return status;
    status = m_OutputDamping.Prepare(format);
    if (!status) return status;
    m_EnableMix.Prepare(format.sampleRate, 12.0, 0.0f);
    m_Amount.Prepare(format.sampleRate, 12.0, 0.0f);
    m_Size.Prepare(format.sampleRate, 12.0, 0.35f);
    m_Diffusion.Prepare(format.sampleRate, 12.0, 0.50f);
    m_Damping.Prepare(format.sampleRate, 12.0, 0.40f);
    m_Width.Prepare(format.sampleRate, 12.0, 0.70f);
    m_Wet.Prepare(format.sampleRate, 12.0, 0.12f);
    m_Prepared = true;
    Reset();
    return {};
}

AudioStatus SpatialHazeProcessor::Publish(
    std::shared_ptr<const SpatialHazeSnapshot> snapshot) {
    if (!snapshot) return {AudioStatusCode::InvalidArgument, 0};
    const SpatialHazeSnapshot* raw = snapshot.get();
    m_Retained.push_back(std::move(snapshot));
    m_Current.store(raw, std::memory_order_release);
    return {};
}

void SpatialHazeProcessor::AdoptSnapshot(const SpatialHazeSnapshot* snapshot) noexcept {
    m_AudioSnapshot = snapshot;
    const SpatialHazeConfig config = snapshot == nullptr
        ? SpatialHazeConfig{}
        : ClampSpatialHazeConfig(snapshot->config);
    m_Enabled.store(config.enabled, std::memory_order_relaxed);
    m_Revision.store(snapshot == nullptr ? 0 : snapshot->revision, std::memory_order_relaxed);
    m_Amount.SetTarget(config.amount);
    m_Size.SetTarget(config.size);
    m_Diffusion.SetTarget(config.diffusion);
    m_Damping.SetTarget(config.damping);
    m_Width.SetTarget(config.width);
    m_Wet.SetTarget(config.wet);
}

void SpatialHazeProcessor::Process(AudioProcessContext& context) noexcept {
    const auto begin = std::chrono::steady_clock::now();
    if (!m_Prepared || context.output.Empty()) return;
    const SpatialHazeSnapshot* published = m_Current.load(std::memory_order_acquire);
    if (published != m_AudioSnapshot) AdoptSnapshot(published);
    const bool active = m_AudioSnapshot != nullptr && m_AudioSnapshot->config.enabled && !Bypassed();
    m_EnableMix.SetTarget(active ? 1.0f : 0.0f);

    std::array<float, 8> dry{};
    std::array<float, 8> wetSignal{};
    for (std::size_t frame = 0; frame < context.output.FrameCount(); ++frame) {
        const float amount = m_Amount.Next();
        const float size = m_Size.Next();
        const float diffusion = m_Diffusion.Next();
        const float damping = m_Damping.Next();
        const float width = m_Width.Next();
        const float wet = m_Wet.Next();
        const float enableMix = m_EnableMix.Next();
        m_CurrentAmount.store(amount, std::memory_order_relaxed);
        m_CurrentSize.store(size, std::memory_order_relaxed);
        m_CurrentDiffusion.store(diffusion, std::memory_order_relaxed);
        m_CurrentDamping.store(damping, std::memory_order_relaxed);
        m_CurrentWidth.store(width, std::memory_order_relaxed);
        m_CurrentWet.store(wet, std::memory_order_relaxed);
        const std::size_t channels = std::min<std::size_t>(context.output.ChannelCount(), dry.size());
        for (std::size_t channel = 0; channel < channels; ++channel) {
            dry[channel] = context.output.Sample(channel, frame);
        }

        const float baseScale = 0.55f + size * 1.25f;
        constexpr std::array<float, kDiffusionNetworkMaximumStages> baseMilliseconds{
            7.0f, 11.0f, 17.0f, 23.0f};
        float maximumDelayMilliseconds = 0.0f;
        for (std::size_t stage = 0; stage < baseMilliseconds.size(); ++stage) {
            const float delayMilliseconds = baseMilliseconds[stage] * baseScale;
            maximumDelayMilliseconds = std::max(maximumDelayMilliseconds, delayMilliseconds);
            const auto delaySamples = std::clamp<std::uint32_t>(
                1u + static_cast<std::uint32_t>(delayMilliseconds * m_Format.sampleRate / 1000.0f),
                1u, m_MaximumDelaySamples);
            m_Network.SetStageParameters(stage, delaySamples,
                0.24f + diffusion * 0.44f, damping);
        }
        m_EffectiveTailMilliseconds.store(
            maximumDelayMilliseconds * (1.0 + diffusion * 2.0), std::memory_order_relaxed);

        if (channels >= 2) {
            const float mid = 0.5f * (dry[0] + dry[1]);
            const float side = 0.5f * (dry[0] - dry[1]) * width;
            dry[0] = mid + side;
            dry[1] = mid - side;
        }
        m_OutputDamping.SetDamping(damping * 0.65f);
        for (std::size_t channel = 0; channel < channels; ++channel) {
            wetSignal[channel] = m_Network.ProcessChannel(channel, dry[channel]);
        }
        m_Network.Advance();
        const float mix = std::clamp(enableMix * wet * amount, 0.0f, 1.0f);
        for (std::size_t channel = 0; channel < channels; ++channel) {
            const float damped = m_OutputDamping.ProcessSample(channel, wetSignal[channel]);
            const float processed = damped * 0.62f;
            context.output.Sample(channel, frame) = std::lerp(
                context.output.Sample(channel, frame), processed, mix);
        }
    }
    const double elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - begin).count();
    m_ProcessMilliseconds.store(elapsed, std::memory_order_relaxed);
    double maximum = m_MaximumProcessMilliseconds.load(std::memory_order_relaxed);
    while (elapsed > maximum && !m_MaximumProcessMilliseconds.compare_exchange_weak(
        maximum, elapsed, std::memory_order_relaxed)) {}
    m_ProcessedFrames.fetch_add(context.output.FrameCount(), std::memory_order_relaxed);
}

void SpatialHazeProcessor::Reset() noexcept {
    m_Network.Reset();
    m_OutputDamping.Reset();
    m_AudioSnapshot = nullptr;
    m_EnableMix.Reset(0.0f);
    m_Amount.Reset(0.0f);
    m_Size.Reset(0.35f);
    m_Diffusion.Reset(0.50f);
    m_Damping.Reset(0.40f);
    m_Width.Reset(0.70f);
    m_Wet.Reset(0.12f);
    m_Enabled.store(false, std::memory_order_release);
    m_Revision.store(0, std::memory_order_release);
    m_ProcessedFrames.store(0, std::memory_order_release);
    m_CurrentAmount.store(0.0f, std::memory_order_release);
    m_CurrentSize.store(0.35f, std::memory_order_release);
    m_CurrentDiffusion.store(0.50f, std::memory_order_release);
    m_CurrentDamping.store(0.40f, std::memory_order_release);
    m_CurrentWidth.store(0.70f, std::memory_order_release);
    m_CurrentWet.store(0.12f, std::memory_order_release);
    m_EffectiveTailMilliseconds.store(0.0, std::memory_order_release);
    m_ProcessMilliseconds.store(0.0, std::memory_order_release);
    m_MaximumProcessMilliseconds.store(0.0, std::memory_order_release);
}

SpatialHazeMetrics SpatialHazeProcessor::CaptureMetrics() const noexcept {
    return {m_Enabled.load(std::memory_order_acquire), Bypassed(),
        m_CurrentAmount.load(std::memory_order_acquire),
        m_CurrentSize.load(std::memory_order_acquire),
        m_CurrentDiffusion.load(std::memory_order_acquire),
        m_CurrentDamping.load(std::memory_order_acquire),
        m_CurrentWidth.load(std::memory_order_acquire),
        m_CurrentWet.load(std::memory_order_acquire),
        m_Revision.load(std::memory_order_acquire),
        m_ProcessedFrames.load(std::memory_order_acquire),
        m_EffectiveTailMilliseconds.load(std::memory_order_acquire),
        0, m_Network.MemoryBytes() + m_OutputDamping.MemoryBytes(),
        m_ProcessMilliseconds.load(std::memory_order_acquire),
        m_MaximumProcessMilliseconds.load(std::memory_order_acquire)};
}

} // namespace Engine::Audio
