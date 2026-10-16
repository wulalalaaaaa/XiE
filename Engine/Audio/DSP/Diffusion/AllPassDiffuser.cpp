#include "Audio/DSP/Diffusion/AllPassDiffuser.h"

#include <algorithm>
#include <cmath>

namespace Engine::Audio {

AudioStatus AllPassDiffuser::Prepare(
    const AudioFormat& format,
    std::uint32_t maximumDelaySamples) {
    const AudioStatus status = m_Delay.Prepare(format, maximumDelaySamples);
    if (!status) return status;
    m_DampingState.assign(format.channelCount, 0.0f);
    m_DelaySamples = std::min<std::uint32_t>(1, maximumDelaySamples);
    m_Feedback = 0.35f;
    m_Damping = 0.0f;
    return {};
}

void AllPassDiffuser::SetParameters(
    std::uint32_t delaySamples,
    float feedback,
    float damping) noexcept {
    m_DelaySamples = std::clamp(delaySamples, 1u, m_Delay.MaximumDelaySamples());
    m_Feedback = std::clamp(std::isfinite(feedback) ? feedback : 0.0f, -0.92f, 0.92f);
    m_Damping = std::clamp(std::isfinite(damping) ? damping : 0.0f, 0.0f, 1.0f);
}

float AllPassDiffuser::ProcessChannel(std::size_t channel, float input) noexcept {
    const float delayed = m_Delay.Read(channel, m_DelaySamples);
    const float coefficient = m_Damping * 0.96f;
    if (channel < m_DampingState.size()) {
        m_DampingState[channel] = delayed * (1.0f - coefficient) +
            m_DampingState[channel] * coefficient;
    }
    const float damped = channel < m_DampingState.size() ? m_DampingState[channel] : delayed;
    const float output = -m_Feedback * input + damped;
    m_Delay.Write(channel, input + m_Feedback * damped);
    return output;
}

void AllPassDiffuser::Reset() noexcept {
    m_Delay.Reset();
    std::fill(m_DampingState.begin(), m_DampingState.end(), 0.0f);
}

} // namespace Engine::Audio
