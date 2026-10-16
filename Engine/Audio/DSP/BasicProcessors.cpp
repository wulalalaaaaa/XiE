#include "Audio/DSP/BasicProcessors.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Engine::Audio {

AudioStatus GainProcessor::Prepare(const AudioFormat& format) {
    if (!format.IsValid()) {
        return {AudioStatusCode::InvalidArgument, 0};
    }
    m_SampleRate = format.sampleRate;
    m_Smoother.Prepare(m_SampleRate, 5.0, m_TargetGain.load(std::memory_order_acquire));
    return {};
}

void GainProcessor::Process(AudioProcessContext& context) noexcept {
    m_Smoother.SetTarget(m_TargetGain.load(std::memory_order_acquire));
    for (std::size_t frame = 0; frame < context.output.FrameCount(); ++frame) {
        const float gain = m_Smoother.Next();
        for (std::size_t channel = 0; channel < context.output.ChannelCount(); ++channel) {
            context.output.Sample(channel, frame) *= gain;
        }
    }
}

void GainProcessor::Reset() noexcept {
    m_Smoother.Reset(m_TargetGain.load(std::memory_order_acquire));
}

AudioStatus BypassProcessor::Prepare(const AudioFormat& format) {
    return format.IsValid() ? AudioStatus{} : AudioStatus{AudioStatusCode::InvalidArgument, 0};
}

void BypassProcessor::Process(AudioProcessContext&) noexcept {
    // The graph already copied input to output; this node deliberately does no work.
}

void ChannelMixer::SetMatrix(std::size_t channels, std::vector<float> gains) {
    m_Channels = channels;
    m_Matrix = std::move(gains);
}

AudioStatus ChannelMixer::Prepare(const AudioFormat& format) {
    if (!format.IsValid() ||
        (m_Channels != 0 && m_Channels != format.channelCount) ||
        (!m_Matrix.empty() && m_Matrix.size() !=
            static_cast<std::size_t>(format.channelCount) * format.channelCount)) {
        return {AudioStatusCode::InvalidArgument, 0};
    }
    m_Channels = format.channelCount;
    m_MaxFrames = format.framesPerBuffer;
    if (m_Matrix.empty()) {
        m_Matrix.assign(m_Channels * m_Channels, 0.0f);
        for (std::size_t channel = 0; channel < m_Channels; ++channel) {
            m_Matrix[channel * m_Channels + channel] = 1.0f;
        }
    }
    m_Scratch.assign(m_Channels * m_MaxFrames, 0.0f);
    return {};
}

void ChannelMixer::Process(AudioProcessContext& context) noexcept {
    const std::size_t frames = context.output.FrameCount();
    for (std::size_t channel = 0; channel < m_Channels; ++channel) {
        std::copy_n(
            context.output.Channel(channel).data(),
            frames,
            m_Scratch.data() + channel * m_MaxFrames);
    }
    for (std::size_t output = 0; output < m_Channels; ++output) {
        for (std::size_t frame = 0; frame < frames; ++frame) {
            float mixed = 0.0f;
            for (std::size_t input = 0; input < m_Channels; ++input) {
                mixed += m_Scratch[input * m_MaxFrames + frame] *
                    m_Matrix[output * m_Channels + input];
            }
            context.output.Sample(output, frame) = mixed;
        }
    }
}

void ChannelMixer::Reset() noexcept {
    std::fill(m_Scratch.begin(), m_Scratch.end(), 0.0f);
}

AudioStatus DCBlocker::Prepare(const AudioFormat& format) {
    if (!format.IsValid()) {
        return {AudioStatusCode::InvalidArgument, 0};
    }
    m_R = std::exp(
        -2.0f * std::numbers::pi_v<float> * m_CutoffHz /
        static_cast<float>(format.sampleRate));
    m_PreviousInput.assign(format.channelCount, 0.0f);
    m_PreviousOutput.assign(format.channelCount, 0.0f);
    return {};
}

void DCBlocker::Process(AudioProcessContext& context) noexcept {
    for (std::size_t channel = 0; channel < context.output.ChannelCount(); ++channel) {
        float previousInput = m_PreviousInput[channel];
        float previousOutput = m_PreviousOutput[channel];
        for (float& sample : context.output.Channel(channel)) {
            const float output = sample - previousInput + m_R * previousOutput;
            previousInput = sample;
            previousOutput = output;
            sample = output;
        }
        m_PreviousInput[channel] = previousInput;
        m_PreviousOutput[channel] = previousOutput;
    }
}

void DCBlocker::Reset() noexcept {
    std::fill(m_PreviousInput.begin(), m_PreviousInput.end(), 0.0f);
    std::fill(m_PreviousOutput.begin(), m_PreviousOutput.end(), 0.0f);
}

AudioStatus Limiter::Prepare(const AudioFormat& format) {
    if (!format.IsValid() || m_Ceiling <= 0.0f || m_Ceiling > 1.0f) {
        return {AudioStatusCode::InvalidArgument, 0};
    }
    const float releaseSamples =
        static_cast<float>(format.sampleRate) * m_ReleaseMilliseconds / 1000.0f;
    m_ReleaseCoefficient = std::exp(-1.0f / std::max(1.0f, releaseSamples));
    m_Gain = 1.0f;
    return {};
}

void Limiter::Process(AudioProcessContext& context) noexcept {
    for (std::size_t frame = 0; frame < context.output.FrameCount(); ++frame) {
        float peak = 0.0f;
        for (std::size_t channel = 0; channel < context.output.ChannelCount(); ++channel) {
            peak = std::max(peak, std::fabs(context.output.Sample(channel, frame)));
        }
        const float required = peak > m_Ceiling ? m_Ceiling / peak : 1.0f;
        if (required < m_Gain) {
            m_Gain = required;
        } else {
            m_Gain = required + m_ReleaseCoefficient * (m_Gain - required);
        }
        for (std::size_t channel = 0; channel < context.output.ChannelCount(); ++channel) {
            context.output.Sample(channel, frame) =
                std::clamp(context.output.Sample(channel, frame) * m_Gain, -m_Ceiling, m_Ceiling);
        }
    }
}

void Limiter::Reset() noexcept {
    m_Gain = 1.0f;
}

AudioStatus MeterProcessor::Prepare(const AudioFormat& format) {
    Reset();
    return format.IsValid() ? AudioStatus{} : AudioStatus{AudioStatusCode::InvalidArgument, 0};
}

void MeterProcessor::Process(AudioProcessContext& context) noexcept {
    float peak = 0.0f;
    double sumSquares = 0.0;
    std::size_t count = 0;
    for (std::size_t channel = 0; channel < context.output.ChannelCount(); ++channel) {
        for (float sample : context.output.Channel(channel)) {
            peak = std::max(peak, std::fabs(sample));
            sumSquares += static_cast<double>(sample) * sample;
            ++count;
        }
    }
    m_Peak.store(peak, std::memory_order_release);
    m_Rms.store(
        count == 0 ? 0.0f : static_cast<float>(std::sqrt(sumSquares / static_cast<double>(count))),
        std::memory_order_release);
}

void MeterProcessor::Reset() noexcept {
    m_Peak.store(0.0f, std::memory_order_release);
    m_Rms.store(0.0f, std::memory_order_release);
}

} // namespace Engine::Audio
