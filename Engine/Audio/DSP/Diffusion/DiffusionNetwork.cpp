#include "Audio/DSP/Diffusion/DiffusionNetwork.h"

namespace Engine::Audio {

AudioStatus DiffusionNetwork::Prepare(
    const AudioFormat& format,
    std::uint32_t maximumDelaySamples) {
    if (!format.IsValid() || maximumDelaySamples == 0) {
        return {AudioStatusCode::InvalidArgument, 0};
    }
    m_MaximumDelaySamples = maximumDelaySamples;
    m_StageCount = kDiffusionNetworkMaximumStages;
    for (AllPassDiffuser& stage : m_Stages) {
        const AudioStatus status = stage.Prepare(format, maximumDelaySamples);
        if (!status) return status;
    }
    return {};
}

void DiffusionNetwork::SetStageParameters(
    std::size_t stage,
    std::uint32_t delaySamples,
    float feedback,
    float damping) noexcept {
    if (stage >= m_StageCount) return;
    m_Stages[stage].SetParameters(delaySamples, feedback, damping);
}

float DiffusionNetwork::ProcessChannel(std::size_t channel, float input) noexcept {
    float value = input;
    for (std::size_t stage = 0; stage < m_StageCount; ++stage) {
        value = m_Stages[stage].ProcessChannel(channel, value);
    }
    return value;
}

void DiffusionNetwork::Advance() noexcept {
    for (std::size_t stage = 0; stage < m_StageCount; ++stage) m_Stages[stage].Advance();
}

void DiffusionNetwork::Reset() noexcept {
    for (AllPassDiffuser& stage : m_Stages) stage.Reset();
}

std::size_t DiffusionNetwork::MemoryBytes() const noexcept {
    std::size_t result = 0;
    for (std::size_t stage = 0; stage < m_StageCount; ++stage) result += m_Stages[stage].MemoryBytes();
    return result;
}

} // namespace Engine::Audio
