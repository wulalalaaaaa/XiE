#include "Audio/DSP/Synthetic/MicroPitchProcessor.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace Engine::Audio {
namespace {

void CopyBlock(AudioBufferView source, AudioBufferView destination) noexcept {
    const std::size_t channels = std::min(source.ChannelCount(), destination.ChannelCount());
    const std::size_t frames = std::min(source.FrameCount(), destination.FrameCount());
    for (std::size_t channel = 0; channel < channels; ++channel) {
        std::copy_n(source.Channel(channel).data(), frames, destination.Channel(channel).data());
    }
}

} // namespace

MicroPitchConfig ClampMicroPitchConfig(MicroPitchConfig config) noexcept {
    config.amount = std::clamp(
        std::isfinite(config.amount) ? config.amount : 0.0f, 0.0f, 1.0f);
    config.character = std::clamp(
        std::isfinite(config.character) ? config.character : 0.0f, 0.0f, 1.0f);
    config.width = std::clamp(
        std::isfinite(config.width) ? config.width : 0.5f, 0.0f, 1.0f);
    config.wet = std::clamp(
        std::isfinite(config.wet) ? config.wet : 1.0f, 0.0f, 1.0f);
    return config;
}

AudioStatus MicroPitchProcessor::Prepare(const AudioFormat& format) {
    if (!format.IsValid()) return {AudioStatusCode::InvalidArgument, 0};
    m_Format = format;
    m_LeftScratch.Resize(format.channelCount, format.framesPerBuffer);
    m_RightScratch.Resize(format.channelCount, format.framesPerBuffer);
    m_LeftShifter.SetWindowFrames(m_WindowFrames);
    m_RightShifter.SetWindowFrames(m_WindowFrames);
    const AudioStatus left = m_LeftShifter.Prepare(format);
    if (!left) return left;
    const AudioStatus right = m_RightShifter.Prepare(format);
    if (!right) return right;
    m_EnableMix.Prepare(format.sampleRate, 12.0, 0.0f);
    m_Amount.Prepare(format.sampleRate, 12.0, 0.0f);
    m_Character.Prepare(format.sampleRate, 12.0, 0.0f);
    m_Width.Prepare(format.sampleRate, 12.0, 0.5f);
    m_Wet.Prepare(format.sampleRate, 12.0, 1.0f);
    m_Prepared = true;
    Reset();
    return {};
}

void MicroPitchProcessor::SetConfig(MicroPitchConfig config, std::uint64_t revision) noexcept {
    m_Config = ClampMicroPitchConfig(config);
    m_SnapshotRevision.store(revision, std::memory_order_release);
}

void MicroPitchProcessor::CopyToScratch(
    AudioBufferView source, AudioBuffer& destination) noexcept {
    CopyBlock(source, destination.View());
}

void MicroPitchProcessor::Process(AudioProcessContext& context) noexcept {
    if (!m_Prepared || context.output.Empty() || Bypassed()) return;
    m_EnableMix.SetTarget(m_Config.enabled ? 1.0f : 0.0f);
    m_Amount.SetTarget(m_Config.amount);
    m_Character.SetTarget(m_Config.character);
    m_Width.SetTarget(m_Config.width);
    m_Wet.SetTarget(m_Config.wet);

    const auto begin = std::chrono::steady_clock::now();
    float enableMix = 0.0f;
    float amount = 0.0f;
    float character = 0.0f;
    float width = 0.5f;
    float wet = 1.0f;
    float blockStartGain = 0.0f;
    float blockEndGain = 0.0f;
    float blockStartWidth = 0.5f;
    float blockEndWidth = 0.5f;
    // Advance the control smoothers once per audio frame, but use the final
    // block value for the parallel layer. This preserves the intended ramp
    // duration without adding per-sample shifter setup work.
    for (std::size_t frame = 0; frame < context.output.FrameCount(); ++frame) {
        enableMix = m_EnableMix.Next();
        amount = m_Amount.Next();
        character = m_Character.Next();
        width = m_Width.Next();
        wet = m_Wet.Next();
        const float gain = enableMix * amount * wet;
        if (frame == 0) {
            blockStartGain = gain;
            blockStartWidth = width;
        }
        blockEndGain = gain;
        blockEndWidth = width;
    }
    m_CurrentAmount.store(amount, std::memory_order_relaxed);
    m_CurrentCharacter.store(character, std::memory_order_relaxed);
    m_CurrentWidth.store(width, std::memory_order_relaxed);
    m_CurrentWet.store(wet, std::memory_order_relaxed);
    const bool processedLayer = std::max(blockStartGain, blockEndGain) > 1.0e-4f;

    // The look-ahead shifters are only run when the smoothed mix is audible.
    // Rewind the smoothers by resetting their target/current is not possible,
    // so process the first sample's values above and use the current values for
    // the complete block. This keeps the hot path bounded and avoids work when
    // the effect is bypassed.
    if (!processedLayer) {
        m_Enabled.store(false, std::memory_order_relaxed);
        m_ProcessedFrames.fetch_add(context.output.FrameCount(), std::memory_order_relaxed);
        return;
    }

    character = m_CurrentCharacter.load(std::memory_order_relaxed);
    const float detuneCents = 2.0f + character * 10.0f;
    m_DetuneCents.store(detuneCents, std::memory_order_relaxed);
    m_DelaySamples.store(m_WindowFrames / 2, std::memory_order_relaxed);
    CopyToScratch(context.output, m_LeftScratch);
    CopyToScratch(context.output, m_RightScratch);
    m_LeftShifter.SetPitchSemitones(-detuneCents / 100.0f);
    m_RightShifter.SetPitchSemitones(detuneCents / 100.0f);
    AudioProcessContext leftContext{
        m_LeftScratch.View(), m_LeftScratch.View(), context.sampleRate, context.absoluteFrame};
    AudioProcessContext rightContext{
        m_RightScratch.View(), m_RightScratch.View(), context.sampleRate, context.absoluteFrame};
    m_LeftShifter.Process(leftContext);
    m_RightShifter.Process(rightContext);

    amount = m_CurrentAmount.load(std::memory_order_relaxed);
    width = m_CurrentWidth.load(std::memory_order_relaxed);
    wet = m_CurrentWet.load(std::memory_order_relaxed);
    for (std::size_t frame = 0; frame < context.output.FrameCount(); ++frame) {
        const float left = m_LeftScratch.View().Sample(0, frame);
        const float right = m_RightScratch.View().Sample(
            std::min<std::size_t>(1, m_RightScratch.ChannelCount() - 1), frame);
        const float mid = 0.5f * (left + right);
        const float interpolation = context.output.FrameCount() > 1
            ? static_cast<float>(frame) /
                static_cast<float>(context.output.FrameCount() - 1)
            : 1.0f;
        const float layerGain = 0.30f * std::lerp(
            blockStartGain, blockEndGain, interpolation);
        const float interpolatedWidth = std::lerp(
            blockStartWidth, blockEndWidth, interpolation);
        const float side = 0.5f * (left - right) * interpolatedWidth;
        for (std::size_t channel = 0; channel < context.output.ChannelCount(); ++channel) {
            const float layer = channel == 0 ? mid + side : mid - side;
            context.output.Sample(channel, frame) += layer * layerGain;
        }
    }
    m_Enabled.store(true, std::memory_order_relaxed);
    m_ProcessedFrames.fetch_add(context.output.FrameCount(), std::memory_order_relaxed);
    const double elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - begin).count();
    m_ProcessMilliseconds.store(elapsed, std::memory_order_relaxed);
    double previous = m_MaximumProcessMilliseconds.load(std::memory_order_relaxed);
    while (elapsed > previous && !m_MaximumProcessMilliseconds.compare_exchange_weak(
        previous, elapsed, std::memory_order_relaxed)) {}
}

void MicroPitchProcessor::Reset() noexcept {
    m_LeftScratch.Clear();
    m_RightScratch.Clear();
    m_LeftShifter.Reset();
    m_RightShifter.Reset();
    m_EnableMix.Reset(0.0f);
    m_Amount.Reset(0.0f);
    m_Character.Reset(0.0f);
    m_Width.Reset(0.5f);
    m_Wet.Reset(1.0f);
    m_Enabled.store(false, std::memory_order_release);
    m_CurrentAmount.store(0.0f, std::memory_order_release);
    m_CurrentCharacter.store(0.0f, std::memory_order_release);
    m_CurrentWidth.store(0.5f, std::memory_order_release);
    m_CurrentWet.store(1.0f, std::memory_order_release);
    m_DetuneCents.store(0.0f, std::memory_order_release);
    m_DelaySamples.store(0, std::memory_order_release);
    m_ProcessedFrames.store(0, std::memory_order_release);
    m_ProcessMilliseconds.store(0.0, std::memory_order_release);
    m_MaximumProcessMilliseconds.store(0.0, std::memory_order_release);
}

MicroPitchMetrics MicroPitchProcessor::CaptureMetrics() const noexcept {
    MicroPitchMetrics metrics;
    metrics.enabled = m_Enabled.load(std::memory_order_acquire);
    metrics.bypassed = Bypassed();
    metrics.amount = m_CurrentAmount.load(std::memory_order_acquire);
    metrics.character = m_CurrentCharacter.load(std::memory_order_acquire);
    metrics.width = m_CurrentWidth.load(std::memory_order_acquire);
    metrics.wet = m_CurrentWet.load(std::memory_order_acquire);
    metrics.detuneCents = m_DetuneCents.load(std::memory_order_acquire);
    metrics.delaySamples = m_DelaySamples.load(std::memory_order_acquire);
    metrics.algorithmicLatencyFrames = metrics.enabled ? LatencyFrames() : 0;
    metrics.processedFrames = m_ProcessedFrames.load(std::memory_order_acquire);
    metrics.snapshotRevision = m_SnapshotRevision.load(std::memory_order_acquire);
    metrics.processMilliseconds = m_ProcessMilliseconds.load(std::memory_order_acquire);
    metrics.maximumProcessMilliseconds =
        m_MaximumProcessMilliseconds.load(std::memory_order_acquire);
    return metrics;
}

} // namespace Engine::Audio
