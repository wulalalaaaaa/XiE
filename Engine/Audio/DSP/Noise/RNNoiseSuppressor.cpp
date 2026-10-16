#include "Audio/DSP/Noise/RNNoiseSuppressor.h"

#include <rnnoise.h>

#include <algorithm>
#include <cmath>

namespace Engine::Audio {
namespace {
constexpr float kPcmScale = 32'768.0f;
}

RNNoiseSuppressor::~RNNoiseSuppressor() {
    if (m_State != nullptr) rnnoise_destroy(m_State);
}

AudioStatus RNNoiseSuppressor::Prepare(const NoiseSuppressorPrepareInfo& info) {
    if (info.streamFormat.sampleRate != RequiredSampleRate() ||
        info.streamFormat.sampleFormat != AudioSampleFormat::Float32) {
        return {AudioStatusCode::UnsupportedFormat, 0};
    }
    if (m_State != nullptr) {
        rnnoise_destroy(m_State);
        m_State = nullptr;
    }
    const int frameSize = rnnoise_get_frame_size();
    if (frameSize <= 0) return {AudioStatusCode::InitializationFailed, 0};
    m_State = rnnoise_create(nullptr);
    if (m_State == nullptr) return {AudioStatusCode::InitializationFailed, 0};
    m_FrameSize = static_cast<std::size_t>(frameSize);
    m_ScaledInput.assign(m_FrameSize, 0.0f);
    m_ScaledOutput.assign(m_FrameSize, 0.0f);
    return {};
}

void RNNoiseSuppressor::Reset() noexcept {
    if (m_State != nullptr) (void)rnnoise_init(m_State, nullptr);
    std::fill(m_ScaledInput.begin(), m_ScaledInput.end(), 0.0f);
    std::fill(m_ScaledOutput.begin(), m_ScaledOutput.end(), 0.0f);
}

std::size_t RNNoiseSuppressor::RequiredFrameSize() const noexcept {
    if (m_FrameSize > 0) return m_FrameSize;
    const int frameSize = rnnoise_get_frame_size();
    return frameSize > 0 ? static_cast<std::size_t>(frameSize) : 0;
}

NoiseFrameResult RNNoiseSuppressor::ProcessFrame(
    const float* input, float* output, std::size_t frames) noexcept {
    if (m_State == nullptr || input == nullptr || output == nullptr ||
        frames != m_FrameSize) {
        return {0.0f, 0.0f, 0.0, false};
    }
    for (std::size_t index = 0; index < frames; ++index) {
        const float sample = std::isfinite(input[index]) ? input[index] : 0.0f;
        m_ScaledInput[index] = std::clamp(sample, -1.0f, 1.0f) * kPcmScale;
    }
    const float probability = rnnoise_process_frame(
        m_State, m_ScaledOutput.data(), m_ScaledInput.data());
    for (std::size_t index = 0; index < frames; ++index) {
        const float sample = m_ScaledOutput[index] / kPcmScale;
        output[index] = std::isfinite(sample) ? std::clamp(sample, -1.0f, 1.0f) : 0.0f;
    }
    return {std::clamp(std::isfinite(probability) ? probability : 0.0f, 0.0f, 1.0f),
        0.0f, 0.0, true};
}

} // namespace Engine::Audio
