#include "Audio/DSP/Pitch.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Engine::Audio {

AudioStatus PitchDetector::Prepare(const AudioFormat& format) {
    if (!format.IsValid() ||
        m_Config.minimumFrequencyHz <= 0.0f ||
        m_Config.maximumFrequencyHz <= m_Config.minimumFrequencyHz ||
        m_Config.windowFrames < 256 ||
        m_Config.analysisHopFrames == 0) {
        return {AudioStatusCode::InvalidArgument, 0};
    }
    m_Format = format;
    m_MinimumLag = std::max<std::uint32_t>(
        2,
        static_cast<std::uint32_t>(
            static_cast<float>(format.sampleRate) / m_Config.maximumFrequencyHz));
    m_MaximumLag = std::min<std::uint32_t>(
        m_Config.windowFrames / 2,
        static_cast<std::uint32_t>(
            static_cast<float>(format.sampleRate) / m_Config.minimumFrequencyHz));
    if (m_MaximumLag <= m_MinimumLag) {
        return {AudioStatusCode::InvalidArgument, 0};
    }
    m_Ring.assign(m_Config.windowFrames, 0.0f);
    m_Window.assign(m_Config.windowFrames, 0.0f);
    m_Difference.assign(m_MaximumLag + 1, 1.0f);
    Reset();
    return {};
}

PitchEstimate PitchDetector::Process(ConstAudioBufferView input) noexcept {
    if (m_Ring.empty() || input.ChannelCount() == 0) {
        return m_Latest;
    }
    for (std::size_t frame = 0; frame < input.FrameCount(); ++frame) {
        float mono = 0.0f;
        for (std::size_t channel = 0; channel < input.ChannelCount(); ++channel) {
            mono += input.Sample(channel, frame);
        }
        mono /= static_cast<float>(input.ChannelCount());
        m_Ring[m_WriteIndex] = mono;
        m_WriteIndex = (m_WriteIndex + 1) % m_Ring.size();
        m_ValidFrames = std::min(m_Ring.size(), m_ValidFrames + 1);
        if (++m_FramesSinceAnalysis >= m_Config.analysisHopFrames &&
            m_ValidFrames == m_Ring.size()) {
            m_FramesSinceAnalysis = 0;
            Analyze();
        }
    }
    return m_Latest;
}

void PitchDetector::Analyze() noexcept {
    for (std::size_t i = 0; i < m_Window.size(); ++i) {
        m_Window[i] = m_Ring[(m_WriteIndex + i) % m_Ring.size()];
    }

    double energy = 0.0;
    for (float sample : m_Window) {
        energy += static_cast<double>(sample) * sample;
    }
    const float rms = static_cast<float>(std::sqrt(energy / static_cast<double>(m_Window.size())));
    if (rms < m_Config.minimumRms) {
        m_Latest = {};
        return;
    }

    const std::size_t comparisonFrames = m_Window.size() - m_MaximumLag;
    m_Difference[0] = 1.0f;
    for (std::uint32_t lag = 1; lag <= m_MaximumLag; ++lag) {
        double sum = 0.0;
        for (std::size_t frame = 0; frame < comparisonFrames; ++frame) {
            const double difference =
                static_cast<double>(m_Window[frame]) - m_Window[frame + lag];
            sum += difference * difference;
        }
        m_Difference[lag] = static_cast<float>(sum);
    }

    double cumulative = 0.0;
    for (std::uint32_t lag = 1; lag <= m_MaximumLag; ++lag) {
        cumulative += m_Difference[lag];
        m_Difference[lag] = cumulative > 0.0
            ? m_Difference[lag] * static_cast<float>(lag) / static_cast<float>(cumulative)
            : 1.0f;
    }

    std::uint32_t selected = 0;
    for (std::uint32_t lag = m_MinimumLag; lag < m_MaximumLag; ++lag) {
        if (m_Difference[lag] < m_Config.yinThreshold) {
            while (lag + 1 <= m_MaximumLag &&
                   m_Difference[lag + 1] < m_Difference[lag]) {
                ++lag;
            }
            selected = lag;
            break;
        }
    }
    if (selected == 0) {
        const auto first = m_Difference.begin() + m_MinimumLag;
        const auto last = m_Difference.begin() + m_MaximumLag + 1;
        selected = static_cast<std::uint32_t>(std::min_element(first, last) - m_Difference.begin());
    }

    float refined = static_cast<float>(selected);
    if (selected > m_MinimumLag && selected < m_MaximumLag) {
        const float left = m_Difference[selected - 1];
        const float center = m_Difference[selected];
        const float right = m_Difference[selected + 1];
        const float denominator = 2.0f * (2.0f * center - right - left);
        if (std::fabs(denominator) > 1.0e-9f) {
            refined += (right - left) / denominator;
        }
    }
    const float confidence = std::clamp(1.0f - m_Difference[selected], 0.0f, 1.0f);
    m_Latest.frequencyHz = static_cast<float>(m_Format.sampleRate) / refined;
    m_Latest.confidence = confidence;
    m_Latest.voiced = confidence >= 0.6f &&
        m_Latest.frequencyHz >= m_Config.minimumFrequencyHz &&
        m_Latest.frequencyHz <= m_Config.maximumFrequencyHz;
    if (!m_Latest.voiced) {
        m_Latest.frequencyHz = 0.0f;
    }
}

void PitchDetector::Reset() noexcept {
    std::fill(m_Ring.begin(), m_Ring.end(), 0.0f);
    std::fill(m_Window.begin(), m_Window.end(), 0.0f);
    m_WriteIndex = 0;
    m_ValidFrames = 0;
    m_FramesSinceAnalysis = 0;
    m_Latest = {};
}

AudioStatus PitchCurve::Publish(std::shared_ptr<const Curve::CurveSnapshot> snapshot) {
    if (!snapshot) {
        return {AudioStatusCode::InvalidArgument, 0};
    }
    const Curve::CurveSnapshot* raw = snapshot.get();
    m_Retained.push_back(std::move(snapshot));
    m_Current.store(raw, std::memory_order_release);
    return {};
}

float PitchCurve::Evaluate(float frequencyHz, float fallbackSemitones) const noexcept {
    const Curve::CurveSnapshot* snapshot = m_Current.load(std::memory_order_acquire);
    return snapshot && frequencyHz > 0.0f ? snapshot->Sample(frequencyHz) : fallbackSemitones;
}

float PitchCurve::HertzToMidi(float frequencyHz) noexcept {
    return frequencyHz > 0.0f ? 69.0f + 12.0f * std::log2(frequencyHz / 440.0f) : 0.0f;
}

float PitchCurve::MidiToHertz(float midiNote) noexcept {
    return 440.0f * std::exp2((midiNote - 69.0f) / 12.0f);
}

AudioStatus PitchShiftProcessor::PreparePitch(const AudioFormat& format) {
    if (!format.IsValid() || m_WindowFrames < 64 || m_WindowFrames > 8192) {
        return {AudioStatusCode::InvalidArgument, 0};
    }
    m_Channels = format.channelCount;
    m_Capacity = static_cast<std::size_t>(m_WindowFrames) + format.framesPerBuffer + 4;
    m_Delay.assign(m_Channels * m_Capacity, 0.0f);
    m_WriteIndex = 0;
    m_Phase = 0.25f;
    m_RatioSmoother.Prepare(
        format.sampleRate,
        20.0,
        std::exp2(m_TargetSemitones.load(std::memory_order_acquire) / 12.0f));
    return {};
}

void PitchShiftProcessor::SetPitchSemitones(float semitones) noexcept {
    m_TargetSemitones.store(std::clamp(semitones, -12.0f, 12.0f), std::memory_order_release);
}

float PitchShiftProcessor::Read(std::size_t channel, float delayFrames) const noexcept {
    float position = static_cast<float>(m_WriteIndex) - delayFrames;
    while (position < 0.0f) {
        position += static_cast<float>(m_Capacity);
    }
    while (position >= static_cast<float>(m_Capacity)) {
        position -= static_cast<float>(m_Capacity);
    }
    const std::size_t left = static_cast<std::size_t>(position);
    const std::size_t right = (left + 1) % m_Capacity;
    const float fraction = position - static_cast<float>(left);
    return std::lerp(
        m_Delay[channel * m_Capacity + left],
        m_Delay[channel * m_Capacity + right],
        fraction);
}

void PitchShiftProcessor::ProcessPitch(AudioProcessContext& context) noexcept {
    if (m_Delay.empty()) {
        return;
    }
    const float targetRatio = std::exp2(
        m_TargetSemitones.load(std::memory_order_acquire) / 12.0f);
    m_RatioSmoother.SetTarget(targetRatio);
    constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
    constexpr float minimumDelay = 16.0f;

    for (std::size_t frame = 0; frame < context.output.FrameCount(); ++frame) {
        const float ratio = m_RatioSmoother.Next();
        if (std::fabs(ratio - 1.0f) < 1.0e-5f) {
            for (std::size_t channel = 0; channel < m_Channels; ++channel) {
                m_Delay[channel * m_Capacity + m_WriteIndex] =
                    context.output.Sample(channel, frame);
            }
        } else {
            float phase2 = m_Phase + 0.5f;
            if (phase2 >= 1.0f) {
                phase2 -= 1.0f;
            }
            const float weight1 = 0.5f - 0.5f * std::cos(twoPi * m_Phase);
            const float weight2 = 1.0f - weight1;
            const float delay1 = minimumDelay + m_Phase * static_cast<float>(m_WindowFrames);
            const float delay2 = minimumDelay + phase2 * static_cast<float>(m_WindowFrames);
            for (std::size_t channel = 0; channel < m_Channels; ++channel) {
                const float input = context.output.Sample(channel, frame);
                m_Delay[channel * m_Capacity + m_WriteIndex] = input;
                context.output.Sample(channel, frame) =
                    Read(channel, delay1) * weight1 + Read(channel, delay2) * weight2;
            }
            m_Phase += (1.0f - ratio) / static_cast<float>(m_WindowFrames);
            m_Phase -= std::floor(m_Phase);
        }
        m_WriteIndex = (m_WriteIndex + 1) % m_Capacity;
    }
}

void PitchShiftProcessor::ResetPitch() noexcept {
    std::fill(m_Delay.begin(), m_Delay.end(), 0.0f);
    m_WriteIndex = 0;
    m_Phase = 0.25f;
    const float ratio = std::exp2(m_TargetSemitones.load(std::memory_order_acquire) / 12.0f);
    m_RatioSmoother.Reset(ratio);
}

} // namespace Engine::Audio
