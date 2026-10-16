#include "Audio/DSP/Synthetic/PitchStabilizer.h"

#include <algorithm>
#include <cmath>

namespace Engine::Audio {

PitchStabilizerConfig ClampPitchStabilizerConfig(
    PitchStabilizerConfig config) noexcept {
    config.stability = std::clamp(
        std::isfinite(config.stability) ? config.stability : 0.0f, 0.0f, 1.0f);
    config.response = std::clamp(
        std::isfinite(config.response) ? config.response : 0.65f, 0.05f, 1.0f);
    config.maxCorrectionCents = std::clamp(
        std::isfinite(config.maxCorrectionCents) ? config.maxCorrectionCents : 12.0f,
        0.0f, 24.0f);
    config.minimumConfidence = std::clamp(
        std::isfinite(config.minimumConfidence) ? config.minimumConfidence : 0.65f,
        0.05f, 1.0f);
    config.holdMilliseconds = std::clamp(
        std::isfinite(config.holdMilliseconds) ? config.holdMilliseconds : 80.0f,
        10.0f, 250.0f);
    return config;
}

void PitchStabilizer::Prepare(const AudioFormat& format) noexcept {
    m_Format = format;
    m_Config = ClampPitchStabilizerConfig(m_Config);
    m_HoldFrames = static_cast<std::uint32_t>(
        static_cast<double>(format.sampleRate) * m_Config.holdMilliseconds / 1000.0);
    m_ControlSmoothingMilliseconds.store(m_Config.holdMilliseconds, std::memory_order_release);
    Reset();
}

void PitchStabilizer::SetConfig(PitchStabilizerConfig config) noexcept {
    m_Config = ClampPitchStabilizerConfig(config);
    m_HoldFrames = static_cast<std::uint32_t>(
        static_cast<double>(m_Format.sampleRate > 0 ? m_Format.sampleRate : 48'000) *
        m_Config.holdMilliseconds / 1000.0);
    m_ControlSmoothingMilliseconds.store(m_Config.holdMilliseconds, std::memory_order_release);
}

PitchStabilizerResult PitchStabilizer::Process(
    PitchEstimate estimate,
    std::uint32_t frameCount) noexcept {
    PitchStabilizerResult result;
    result.rawFrequencyHz = std::isfinite(estimate.frequencyHz) ? estimate.frequencyHz : 0.0f;
    result.confidence = std::clamp(
        std::isfinite(estimate.confidence) ? estimate.confidence : 0.0f, 0.0f, 1.0f);
    result.voiced = estimate.voiced && result.rawFrequencyHz > 1.0f;

    if (!m_Config.enabled || m_Config.stability <= 1.0e-4f) {
        m_StabilizedFrequencyHz = result.rawFrequencyHz;
        m_HasFrequency = result.rawFrequencyHz > 1.0f;
        m_InvalidFrames = 0;
        result.stabilizedFrequencyHz = result.rawFrequencyHz;
        m_Latest = result;
        return result;
    }

    const bool valid = result.voiced && result.confidence >= m_Config.minimumConfidence;
    if (valid) {
        if (!m_HasFrequency || m_StabilizedFrequencyHz <= 1.0f) {
            m_StabilizedFrequencyHz = result.rawFrequencyHz;
            m_HasFrequency = true;
            m_InvalidFrames = 0;
            result.stabilizedFrequencyHz = m_StabilizedFrequencyHz;
            m_Latest = result;
            return result;
        }

        m_InvalidFrames = 0;
        const float ratio = std::max(
            1.0e-5f, result.rawFrequencyHz / std::max(1.0f, m_StabilizedFrequencyHz));
        const float deltaCents = 1200.0f * std::log2(ratio);
        // The threshold makes the stabilizer preserve deliberate pitch motion
        // while treating small movement as detector jitter/drift.
        const float largeMoveThreshold = 24.0f + (1.0f - m_Config.response) * 42.0f;
        const float trendAlpha = 0.16f + 0.62f * m_Config.response;
        const float jitterAlpha = 0.025f +
            0.30f * m_Config.response * (1.0f - m_Config.stability);
        const float alpha = std::abs(deltaCents) >= largeMoveThreshold
            ? trendAlpha
            : jitterAlpha;
        m_StabilizedFrequencyHz +=
            (result.rawFrequencyHz - m_StabilizedFrequencyHz) * std::clamp(alpha, 0.01f, 0.86f);
        const float stabilizedRatio = std::max(
            1.0e-5f, m_StabilizedFrequencyHz / std::max(1.0f, result.rawFrequencyHz));
        result.correctionCents = std::clamp(
            1200.0f * std::log2(stabilizedRatio) * m_Config.stability,
            -m_Config.maxCorrectionCents, m_Config.maxCorrectionCents);
        result.stabilizedFrequencyHz = m_StabilizedFrequencyHz;
        m_Latest = result;
        return result;
    }

    // Confidence loss does not jump the correction. It holds the last stable
    // control briefly, then fades the correction to zero for unvoiced input.
    m_InvalidFrames = std::min<std::uint32_t>(
        m_HoldFrames + std::max<std::uint32_t>(1, frameCount),
        m_InvalidFrames + std::max<std::uint32_t>(1, frameCount));
    result.held = m_HasFrequency && m_InvalidFrames <= m_HoldFrames;
    result.stabilizedFrequencyHz = m_HasFrequency ? m_StabilizedFrequencyHz : 0.0f;
    const float fade = m_HoldFrames == 0
        ? 0.0f
        : std::clamp(static_cast<float>(m_InvalidFrames) /
            static_cast<float>(m_HoldFrames), 0.0f, 1.0f);
    result.correctionCents = m_Latest.correctionCents * (1.0f - fade);
    if (!result.held) {
        result.correctionCents = 0.0f;
    }
    m_Latest = result;
    return result;
}

void PitchStabilizer::Reset() noexcept {
    m_Latest = {};
    m_StabilizedFrequencyHz = 0.0f;
    m_HasFrequency = false;
    m_InvalidFrames = 0;
}

} // namespace Engine::Audio
