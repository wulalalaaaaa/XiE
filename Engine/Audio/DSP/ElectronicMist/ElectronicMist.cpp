#include "Audio/DSP/ElectronicMist/ElectronicMist.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace Engine::Audio {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr std::array<float, kElectronicMistBandCount> kBandCenters{
    120.0f, 250.0f, 500.0f, 900.0f, 1'600.0f, 3'000.0f, 6'000.0f, 11'000.0f};

float Finite(float value, float fallback) noexcept {
    return std::isfinite(value) ? value : fallback;
}

float SmoothEnvelope(float current, float target, float attack, float release) noexcept {
    const float coefficient = target > current ? attack : release;
    return current + (target - current) * coefficient;
}

} // namespace

ElectronicMistConfig ClampElectronicMistConfig(ElectronicMistConfig config) noexcept {
    config.amount = std::clamp(Finite(config.amount, 0.0f), 0.0f, 1.0f);
    config.follow = std::clamp(Finite(config.follow, 0.75f), 0.0f, 1.0f);
    config.sharpness = std::clamp(Finite(config.sharpness, 0.40f), 0.0f, 1.0f);
    config.width = std::clamp(Finite(config.width, 0.40f), 0.0f, 1.0f);
    config.wet = std::clamp(Finite(config.wet, 1.0f), 0.0f, 1.0f);
    return config;
}

float ElectronicMistSnapshot::SampleCurve(float frequencyHz) const noexcept {
    if (curveSize == 0) return 0.0f;
    const float minimum = kElectronicMistMinimumFrequencyHz;
    const float maximum = kElectronicMistMaximumFrequencyHz;
    const float safeFrequency = std::clamp(Finite(frequencyHz, minimum), minimum, maximum);
    const float normalized = std::clamp(
        std::log(safeFrequency / minimum) / std::log(maximum / minimum), 0.0f, 1.0f);
    const float position = normalized * static_cast<float>(curveSize - 1);
    const std::size_t left = static_cast<std::size_t>(position);
    const std::size_t right = std::min(left + 1, curveSize - 1);
    return std::clamp(std::lerp(curve[left], curve[right],
        position - static_cast<float>(left)), 0.0f, 1.0f);
}

std::shared_ptr<const ElectronicMistSnapshot> BuildElectronicMistSnapshot(
    ElectronicMistConfig config,
    std::shared_ptr<const Curve::CurveSnapshot> curve,
    std::uint64_t revision) {
    auto snapshot = std::make_shared<ElectronicMistSnapshot>();
    snapshot->config = ClampElectronicMistConfig(config);
    snapshot->revision = revision;
    snapshot->curveSize = kElectronicMistCurveLutSize;
    for (std::size_t index = 0; index < snapshot->curveSize; ++index) {
        const float normalized = static_cast<float>(index) /
            static_cast<float>(snapshot->curveSize - 1);
        const float frequency = kElectronicMistMinimumFrequencyHz * std::pow(
            kElectronicMistMaximumFrequencyHz / kElectronicMistMinimumFrequencyHz, normalized);
        snapshot->curve[index] = curve ? std::clamp(curve->Sample(frequency), 0.0f, 1.0f) : 0.0f;
    }
    return snapshot;
}

std::shared_ptr<const ElectronicMistSnapshot> BuildElectronicMistSnapshot(
    ElectronicMistConfig config,
    std::uint64_t revision) {
    return BuildElectronicMistSnapshot(std::move(config), {}, revision);
}

void ElectronicMistNoiseGenerator::Prepare(std::size_t channels) noexcept {
    m_Channels = std::clamp<std::size_t>(channels, 1, m_Previous.size());
    Reset();
}

void ElectronicMistNoiseGenerator::Reset() noexcept {
    m_State = m_Seed == 0 ? 0x13579BDFu : m_Seed;
    m_Previous.fill(0.0f);
    m_DigitalPrevious.fill(0.0f);
}

float ElectronicMistNoiseGenerator::NextUnit() noexcept {
    std::uint32_t state = m_State;
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    m_State = state == 0 ? 0x13579BDFu : state;
    return static_cast<float>(m_State & 0x00FFFFFFu) / 8'388'607.5f - 1.0f;
}

float ElectronicMistNoiseGenerator::Next(
    std::size_t channel,
    ElectronicMistNoiseType type) noexcept {
    const std::size_t safeChannel = std::min(channel, m_Previous.size() - 1);
    const float white = NextUnit();
    switch (type) {
    case ElectronicMistNoiseType::White:
        return white;
    case ElectronicMistNoiseType::Digital: {
        const float quantized = std::round(white * 15.0f) / 15.0f;
        const float value = quantized - m_DigitalPrevious[safeChannel] * 0.35f;
        m_DigitalPrevious[safeChannel] = quantized;
        return value;
    }
    case ElectronicMistNoiseType::Soft:
    default: {
        const float value = white * 0.28f + m_Previous[safeChannel] * 0.72f;
        m_Previous[safeChannel] = value;
        return value;
    }
    }
}

AudioStatus ElectronicMistVoiceBandFollower::Prepare(const AudioFormat& format) noexcept {
    if (!format.IsValid()) return {AudioStatusCode::InvalidArgument, 0};
    m_Format = format;
    m_AttackCoefficient = 1.0f - std::exp(-1.0f /
        (static_cast<float>(format.sampleRate) * 0.008f));
    m_ReleaseCoefficient = 1.0f - std::exp(-1.0f /
        (static_cast<float>(format.sampleRate) * 0.080f));
    for (std::size_t index = 0; index < kElectronicMistBandCount; ++index) {
        BandState& band = m_Bands[index];
        band = {};
        band.centerHz = std::min(kBandCenters[index],
            static_cast<float>(format.sampleRate) * 0.45f);
        const float omega = 2.0f * kPi * band.centerHz / static_cast<float>(format.sampleRate);
        const float alpha = std::sin(omega) / (2.0f * 0.85f);
        const float a0 = 1.0f + alpha;
        band.b0 = alpha / a0;
        band.b1 = 0.0f;
        band.b2 = -alpha / a0;
        band.a1 = (-2.0f * std::cos(omega)) / a0;
        band.a2 = (1.0f - alpha) / a0;
    }
    m_Prepared = true;
    Reset();
    return {};
}

void ElectronicMistVoiceBandFollower::Reset() noexcept {
    for (BandState& band : m_Bands) {
        band.z1 = 0.0f;
        band.z2 = 0.0f;
        band.envelope = 0.0f;
    }
    m_BroadbandEnvelope = 0.0f;
    m_GateActivity = 0.0f;
}

void ElectronicMistVoiceBandFollower::ProcessSample(
    ConstAudioBufferView input,
    std::size_t frame,
    bool voiced,
    float confidence) noexcept {
    if (!m_Prepared || input.Data() == nullptr || input.FrameCount() == 0 ||
        frame >= input.FrameCount()) return;
    float mono = 0.0f;
    const std::size_t channels = std::max<std::size_t>(1, input.ChannelCount());
    for (std::size_t channel = 0; channel < input.ChannelCount(); ++channel) {
        mono += input.Sample(channel, frame);
    }
    mono /= static_cast<float>(channels);
    const float absolute = std::abs(mono);
    m_BroadbandEnvelope = SmoothEnvelope(m_BroadbandEnvelope, absolute,
        m_AttackCoefficient, m_ReleaseCoefficient);
    const float broadbandGate = std::clamp((m_BroadbandEnvelope - 0.0008f) * 55.0f, 0.0f, 1.0f);
    const float pitchGate = voiced ? std::clamp(0.35f + confidence * 0.65f, 0.35f, 1.0f) : 0.72f;
    const float targetGate = broadbandGate * pitchGate;
    // The voice gate releases faster than the per-band energy followers so a
    // stopped phrase does not leave an audible noise tail.
    m_GateActivity = SmoothEnvelope(m_GateActivity, targetGate,
        m_AttackCoefficient, std::min(1.0f, m_ReleaseCoefficient * 4.0f));

    for (BandState& band : m_Bands) {
        const float filtered = band.b0 * mono + band.z1;
        band.z1 = band.b1 * mono - band.a1 * filtered + band.z2;
        band.z2 = band.b2 * mono - band.a2 * filtered;
        band.envelope = SmoothEnvelope(band.envelope, std::abs(filtered),
            m_AttackCoefficient, m_ReleaseCoefficient);
    }
}

AudioStatus ElectronicMistProcessor::Prepare(const AudioFormat& format) {
    if (!format.IsValid()) return {AudioStatusCode::InvalidArgument, 0};
    m_Format = format;
    m_VoiceBuffer.Resize(format.channelCount, format.framesPerBuffer);
    AudioStatus status = m_Follower.Prepare(format);
    if (!status) return status;
    m_Noise.Prepare(format.channelCount);
    for (auto& smoother : m_Smoothers) smoother.Prepare(format.sampleRate, 8.0, 0.0f);
    m_Smoothers[1].Reset(0.75f);
    m_Smoothers[2].Reset(0.40f);
    m_Smoothers[3].Reset(0.40f);
    m_Smoothers[4].Reset(1.0f);
    m_Prepared = true;
    Reset();
    return {};
}

AudioStatus ElectronicMistProcessor::Publish(
    std::shared_ptr<const ElectronicMistSnapshot> snapshot) {
    if (!snapshot) return {AudioStatusCode::InvalidArgument, 0};
    const ElectronicMistSnapshot* raw = snapshot.get();
    m_Retained.push_back(std::move(snapshot));
    m_Current.store(raw, std::memory_order_release);
    return {};
}

void ElectronicMistProcessor::AdoptSnapshot(const ElectronicMistSnapshot* snapshot) noexcept {
    m_AudioSnapshot = snapshot;
    m_Config = snapshot ? ClampElectronicMistConfig(snapshot->config) : ElectronicMistConfig{};
    m_Enabled.store(m_Config.enabled, std::memory_order_relaxed);
    m_Revision.store(snapshot ? snapshot->revision : 0, std::memory_order_relaxed);
    m_Smoothers[0].SetTarget(m_Config.amount);
    m_Smoothers[1].SetTarget(m_Config.follow);
    m_Smoothers[2].SetTarget(m_Config.sharpness);
    m_Smoothers[3].SetTarget(m_Config.width);
    m_Smoothers[4].SetTarget(m_Config.wet);
}

void ElectronicMistProcessor::Process(AudioProcessContext& context) noexcept {
    const auto begin = std::chrono::steady_clock::now();
    if (!m_Prepared || context.output.Empty()) return;
    const ElectronicMistSnapshot* published = m_Current.load(std::memory_order_acquire);
    if (published != m_AudioSnapshot) AdoptSnapshot(published);
    const bool active = m_AudioSnapshot != nullptr && m_Config.enabled && !Bypassed();
    if (!active) {
        // Disabled/explicitly bypassed Mist is a true dry fast path.  In
        // particular, do not run the follower bank or advance noise state when
        // the user has not enabled the effect.
        m_BroadbandEnvelope.store(0.0f, std::memory_order_relaxed);
        m_GateActivity.store(0.0f, std::memory_order_relaxed);
        m_ActiveBandCount.store(0, std::memory_order_relaxed);
        m_AverageNoiseGain.store(0.0f, std::memory_order_relaxed);
        m_PeakNoiseGain.store(0.0f, std::memory_order_relaxed);
        m_MistRms.store(0.0f, std::memory_order_relaxed);
        m_VoiceRms.store(0.0f, std::memory_order_relaxed);
        m_MistToVoiceDb.store(-100.0f, std::memory_order_relaxed);
        m_ProcessedFrames.fetch_add(context.output.FrameCount(), std::memory_order_relaxed);
        return;
    }
    double mistSquare = 0.0;
    double voiceSquare = 0.0;
    float gainSum = 0.0f;
    float gainPeak = 0.0f;
    std::size_t activeBands = 0;
    AudioBufferView voiceBuffer = m_VoiceBuffer.View();
    const std::size_t copiedChannels = std::min(voiceBuffer.ChannelCount(),
        context.output.ChannelCount());
    const std::size_t copiedFrames = std::min(voiceBuffer.FrameCount(),
        context.output.FrameCount());
    for (std::size_t channel = 0; channel < copiedChannels; ++channel) {
        std::copy_n(context.output.Channel(channel).data(), copiedFrames,
            voiceBuffer.Channel(channel).data());
    }
    const ConstAudioBufferView input = static_cast<const AudioBuffer&>(m_VoiceBuffer).View();
    for (std::size_t frame = 0; frame < context.output.FrameCount(); ++frame) {
        const float amount = m_Smoothers[0].Next();
        const float follow = m_Smoothers[1].Next();
        const float sharpness = m_Smoothers[2].Next();
        const float width = m_Smoothers[3].Next();
        const float wet = m_Smoothers[4].Next();
        m_Amount.store(amount, std::memory_order_relaxed);
        m_Follow.store(follow, std::memory_order_relaxed);
        m_Sharpness.store(sharpness, std::memory_order_relaxed);
        m_Width.store(width, std::memory_order_relaxed);
        m_Wet.store(wet, std::memory_order_relaxed);
        m_Follower.ProcessSample(input, frame, m_Voiced, m_Confidence);
        const float gate = active ? m_Follower.GateActivity() : 0.0f;
        const float broad = m_Follower.BroadbandEnvelope();
        voiceSquare += static_cast<double>(broad) * broad;
        float mist = 0.0f;
        const float normalization = 1.0f / std::sqrt(static_cast<float>(kElectronicMistBandCount));
        for (std::size_t band = 0; band < kElectronicMistBandCount; ++band) {
            const float bandEnvelope = m_Follower.Envelope(band);
            const float relative = std::clamp(bandEnvelope * 4.0f /
                std::max(0.001f, broad), 0.0f, 1.0f);
            const float followGain = std::lerp(1.0f, relative, follow);
            const float curveGain = m_AudioSnapshot
                ? m_AudioSnapshot->SampleCurve(m_Follower.CenterFrequency(band)) : 0.0f;
            const float frequencyT = std::clamp(std::log(
                m_Follower.CenterFrequency(band) / kElectronicMistMinimumFrequencyHz) /
                std::log(kElectronicMistMaximumFrequencyHz / kElectronicMistMinimumFrequencyHz),
                0.0f, 1.0f);
            const float sharpGain = 1.0f + sharpness * 1.8f * frequencyT;
            const float gain = amount * curveGain * followGain * sharpGain * gate * 0.18f;
            if (gain > 0.001f) ++activeBands;
            gainSum += gain;
            gainPeak = std::max(gainPeak, gain);
            for (std::size_t channel = 0; channel < context.output.ChannelCount(); ++channel) {
                const float same = m_Noise.Next(0, m_Config.noiseType);
                const float own = m_Noise.Next(channel, m_Config.noiseType);
                const float noise = std::lerp(same, own, width);
                if (channel == 0) mist += noise * gain * normalization;
            }
        }
        mist *= wet;
        mistSquare += static_cast<double>(mist) * mist;
        if (active) {
            for (std::size_t channel = 0; channel < context.output.ChannelCount(); ++channel) {
                const float same = m_Noise.Next(0, m_Config.noiseType);
                const float own = m_Noise.Next(channel, m_Config.noiseType);
                context.output.Sample(channel, frame) += std::lerp(same, own, width) *
                    (mist * 0.65f);
            }
        }
    }
    const float frameScale = context.output.FrameCount() > 0
        ? 1.0f / static_cast<float>(context.output.FrameCount()) : 0.0f;
    const float mistRms = static_cast<float>(std::sqrt(mistSquare * frameScale));
    const float voiceRms = static_cast<float>(std::sqrt(voiceSquare * frameScale));
    const float ratioDb = mistRms > 1.0e-7f && voiceRms > 1.0e-7f
        ? 20.0f * std::log10(mistRms / voiceRms) : -100.0f;
    m_BroadbandEnvelope.store(m_Follower.BroadbandEnvelope(), std::memory_order_relaxed);
    m_GateActivity.store(m_Follower.GateActivity(), std::memory_order_relaxed);
    m_ActiveBandCount.store(activeBands, std::memory_order_relaxed);
    const float gainDenominator = static_cast<float>(
        std::max<std::size_t>(1, context.output.FrameCount()) * kElectronicMistBandCount);
    m_AverageNoiseGain.store(gainSum / gainDenominator, std::memory_order_relaxed);
    m_PeakNoiseGain.store(gainPeak, std::memory_order_relaxed);
    m_MistRms.store(mistRms, std::memory_order_relaxed);
    m_VoiceRms.store(voiceRms, std::memory_order_relaxed);
    m_MistToVoiceDb.store(ratioDb, std::memory_order_relaxed);
    const double elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - begin).count();
    m_ProcessMilliseconds.store(elapsed, std::memory_order_relaxed);
    double maximum = m_MaximumProcessMilliseconds.load(std::memory_order_relaxed);
    while (elapsed > maximum && !m_MaximumProcessMilliseconds.compare_exchange_weak(
        maximum, elapsed, std::memory_order_relaxed)) {}
    m_ProcessedFrames.fetch_add(context.output.FrameCount(), std::memory_order_relaxed);
}

void ElectronicMistProcessor::Reset() noexcept {
    m_Follower.Reset();
    m_Noise.Reset();
    for (auto& smoother : m_Smoothers) smoother.Reset(0.0f);
    m_Smoothers[1].Reset(0.75f);
    m_Smoothers[2].Reset(0.40f);
    m_Smoothers[3].Reset(0.40f);
    m_Smoothers[4].Reset(1.0f);
    m_AudioSnapshot = nullptr;
    m_Enabled.store(false, std::memory_order_release);
    m_Amount.store(0.0f, std::memory_order_release);
    m_Follow.store(0.75f, std::memory_order_release);
    m_Sharpness.store(0.40f, std::memory_order_release);
    m_Width.store(0.40f, std::memory_order_release);
    m_Wet.store(1.0f, std::memory_order_release);
    m_BroadbandEnvelope.store(0.0f, std::memory_order_release);
    m_GateActivity.store(0.0f, std::memory_order_release);
    m_ActiveBandCount.store(0, std::memory_order_release);
    m_AverageNoiseGain.store(0.0f, std::memory_order_release);
    m_PeakNoiseGain.store(0.0f, std::memory_order_release);
    m_MistRms.store(0.0f, std::memory_order_release);
    m_VoiceRms.store(0.0f, std::memory_order_release);
    m_MistToVoiceDb.store(-100.0f, std::memory_order_release);
    m_Revision.store(0, std::memory_order_release);
    m_ProcessedFrames.store(0, std::memory_order_release);
    m_ProcessMilliseconds.store(0.0, std::memory_order_release);
    m_MaximumProcessMilliseconds.store(0.0, std::memory_order_release);
}

ElectronicMistMetrics ElectronicMistProcessor::CaptureMetrics() const noexcept {
    return {m_Enabled.load(std::memory_order_acquire), Bypassed(),
        m_Amount.load(std::memory_order_acquire), m_Follow.load(std::memory_order_acquire),
        m_Sharpness.load(std::memory_order_acquire), m_Width.load(std::memory_order_acquire),
        m_Wet.load(std::memory_order_acquire),
        m_BroadbandEnvelope.load(std::memory_order_acquire),
        m_GateActivity.load(std::memory_order_acquire), kElectronicMistBandCount,
        m_ActiveBandCount.load(std::memory_order_acquire),
        m_AverageNoiseGain.load(std::memory_order_acquire),
        m_PeakNoiseGain.load(std::memory_order_acquire),
        m_MistRms.load(std::memory_order_acquire), m_VoiceRms.load(std::memory_order_acquire),
        m_MistToVoiceDb.load(std::memory_order_acquire),
        m_Revision.load(std::memory_order_acquire),
        m_ProcessedFrames.load(std::memory_order_acquire), 0,
        m_ProcessMilliseconds.load(std::memory_order_acquire),
        m_MaximumProcessMilliseconds.load(std::memory_order_acquire)};
}

} // namespace Engine::Audio
