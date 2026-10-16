#include "Audio/DSP/Formant.h"

#include <algorithm>
#include <cmath>

namespace Engine::Audio {

AudioStatus FormantShiftProcessor::PrepareFormant(const AudioFormat& format) {
    if (!format.IsValid()) {
        return {AudioStatusCode::InvalidArgument, 0};
    }
    m_PreviousInputA.assign(format.channelCount, 0.0f);
    m_PreviousOutputA.assign(format.channelCount, 0.0f);
    m_PreviousInputB.assign(format.channelCount, 0.0f);
    m_PreviousOutputB.assign(format.channelCount, 0.0f);
    m_AmountSmoother.Prepare(format.sampleRate, 30.0, 0.0f);
    return {};
}

void FormantShiftProcessor::SetFormantSemitones(float semitones) noexcept {
    m_TargetSemitones.store(std::clamp(semitones, -6.0f, 6.0f), std::memory_order_release);
}

void FormantShiftProcessor::ProcessFormant(AudioProcessContext& context) noexcept {
    const float targetSemitones = m_TargetSemitones.load(std::memory_order_acquire);
    m_AmountSmoother.SetTarget(std::clamp(targetSemitones / 6.0f, -1.0f, 1.0f));

    for (std::size_t frame = 0; frame < context.output.FrameCount(); ++frame) {
        const float amount = m_AmountSmoother.Next();
        if (std::fabs(amount) < 1.0e-5f) {
            continue;
        }
        // Two cascaded first-order all-pass sections create a smoothly warped
        // phase response. Mixing their residual with the dry signal moves broad
        // spectral-envelope emphasis without changing stream duration.
        const float coefficient = std::clamp(amount * 0.55f, -0.55f, 0.55f);
        for (std::size_t channel = 0; channel < context.output.ChannelCount(); ++channel) {
            const float input = context.output.Sample(channel, frame);
            const float allPassA =
                coefficient * input + m_PreviousInputA[channel] -
                coefficient * m_PreviousOutputA[channel];
            m_PreviousInputA[channel] = input;
            m_PreviousOutputA[channel] = allPassA;

            const float allPassB =
                coefficient * allPassA + m_PreviousInputB[channel] -
                coefficient * m_PreviousOutputB[channel];
            m_PreviousInputB[channel] = allPassA;
            m_PreviousOutputB[channel] = allPassB;

            const float residual = input - allPassB;
            context.output.Sample(channel, frame) =
                std::clamp(input + amount * 0.6f * residual, -2.0f, 2.0f);
        }
    }
}

void FormantShiftProcessor::ResetFormant() noexcept {
    std::fill(m_PreviousInputA.begin(), m_PreviousInputA.end(), 0.0f);
    std::fill(m_PreviousOutputA.begin(), m_PreviousOutputA.end(), 0.0f);
    std::fill(m_PreviousInputB.begin(), m_PreviousInputB.end(), 0.0f);
    std::fill(m_PreviousOutputB.begin(), m_PreviousOutputB.end(), 0.0f);
    m_AmountSmoother.Reset(0.0f);
}

AudioStatus FormantCurve::Publish(std::shared_ptr<const Curve::CurveSnapshot> snapshot) {
    if (!snapshot) {
        return {AudioStatusCode::InvalidArgument, 0};
    }
    const Curve::CurveSnapshot* raw = snapshot.get();
    m_Retained.push_back(std::move(snapshot));
    m_Current.store(raw, std::memory_order_release);
    return {};
}

float FormantCurve::Evaluate(float frequencyHz, float fallbackSemitones) const noexcept {
    const Curve::CurveSnapshot* snapshot = m_Current.load(std::memory_order_acquire);
    return snapshot && frequencyHz > 0.0f ? snapshot->Sample(frequencyHz) : fallbackSemitones;
}

} // namespace Engine::Audio
