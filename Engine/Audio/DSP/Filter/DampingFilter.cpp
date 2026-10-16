#include "Audio/DSP/Filter/DampingFilter.h"

#include <algorithm>
#include <cmath>

namespace Engine::Audio {

AudioStatus DampingFilter::Prepare(const AudioFormat& format) {
    if (!format.IsValid()) return {AudioStatusCode::InvalidArgument, 0};
    m_State.assign(format.channelCount, 0.0f);
    m_Damping = 0.0f;
    return {};
}

void DampingFilter::SetDamping(float damping) noexcept {
    m_Damping = std::clamp(std::isfinite(damping) ? damping : 0.0f, 0.0f, 1.0f);
}

float DampingFilter::ProcessSample(std::size_t channel, float input) noexcept {
    if (channel >= m_State.size()) return input;
    // At zero damping this is an exact bypass. Increasing damping retains
    // more of the previous low-passed state and softens the high end.
    const float coefficient = m_Damping * 0.96f;
    m_State[channel] = input * (1.0f - coefficient) + m_State[channel] * coefficient;
    return m_State[channel];
}

void DampingFilter::Reset() noexcept {
    std::fill(m_State.begin(), m_State.end(), 0.0f);
}

} // namespace Engine::Audio
