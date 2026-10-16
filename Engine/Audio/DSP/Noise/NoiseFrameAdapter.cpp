#include "Audio/DSP/Noise/NoiseFrameAdapter.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace Engine::Audio {

AudioStatus NoiseFrameAdapter::Prepare(std::size_t algorithmFrameSize) {
    if (algorithmFrameSize == 0 || algorithmFrameSize > 16'384) {
        return {AudioStatusCode::InvalidArgument, 0};
    }
    m_FrameSize = algorithmFrameSize;
    m_InputFrame.assign(m_FrameSize, 0.0f);
    m_AlgorithmOutput.assign(m_FrameSize, 0.0f);
    m_ProcessedRing.assign(m_FrameSize, 0.0f);
    m_DryDelay.assign(m_FrameSize, 0.0f);
    Reset();
    return {};
}

void NoiseFrameAdapter::Reset() noexcept {
    std::fill(m_InputFrame.begin(), m_InputFrame.end(), 0.0f);
    std::fill(m_AlgorithmOutput.begin(), m_AlgorithmOutput.end(), 0.0f);
    std::fill(m_ProcessedRing.begin(), m_ProcessedRing.end(), 0.0f);
    std::fill(m_DryDelay.begin(), m_DryDelay.end(), 0.0f);
    m_InputCount = 0;
    m_ProcessedRead = 0;
    m_ProcessedWrite = 0;
    m_ProcessedCount = 0;
    m_DryRead = 0;
    m_DryWrite = 0;
    m_DryCount = 0;
    m_InputFrames = 0;
    m_OutputFrames = 0;
    m_AlgorithmFrames = 0;
}

bool NoiseFrameAdapter::PopProcessed(float& value) noexcept {
    if (m_ProcessedCount == 0) return false;
    value = m_ProcessedRing[m_ProcessedRead];
    m_ProcessedRead = (m_ProcessedRead + 1) % m_FrameSize;
    --m_ProcessedCount;
    return true;
}

void NoiseFrameAdapter::PushProcessed(std::span<const float> values) noexcept {
    for (float value : values) {
        if (m_ProcessedCount >= m_FrameSize) return;
        m_ProcessedRing[m_ProcessedWrite] = std::isfinite(value) ? value : 0.0f;
        m_ProcessedWrite = (m_ProcessedWrite + 1) % m_FrameSize;
        ++m_ProcessedCount;
    }
}

float NoiseFrameAdapter::PopDelayedDry() noexcept {
    if (m_DryCount < m_FrameSize) return 0.0f;
    const float value = m_DryDelay[m_DryRead];
    m_DryRead = (m_DryRead + 1) % m_FrameSize;
    --m_DryCount;
    return value;
}

void NoiseFrameAdapter::PushDelayedDry(float value) noexcept {
    if (m_DryCount >= m_FrameSize) return;
    m_DryDelay[m_DryWrite] = value;
    m_DryWrite = (m_DryWrite + 1) % m_FrameSize;
    ++m_DryCount;
}

NoiseFrameResult NoiseFrameAdapter::Process(
    std::span<const float> monoInput,
    std::span<float> delayedDryOutput,
    std::span<float> processedOutput,
    INoiseSuppressor& suppressor) noexcept {
    NoiseFrameResult latest{};
    double attenuationSumDb = 0.0;
    if (!Prepared() || delayedDryOutput.size() < monoInput.size() ||
        processedOutput.size() < monoInput.size() ||
        suppressor.RequiredFrameSize() != m_FrameSize) {
        latest.valid = false;
        return latest;
    }
    for (std::size_t index = 0; index < monoInput.size(); ++index) {
        float processed = 0.0f;
        (void)PopProcessed(processed);
        processedOutput[index] = processed;
        delayedDryOutput[index] = PopDelayedDry();

        const float input = std::isfinite(monoInput[index]) ? monoInput[index] : 0.0f;
        PushDelayedDry(input);
        m_InputFrame[m_InputCount++] = input;
        ++m_InputFrames;
        ++m_OutputFrames;
        if (m_InputCount == m_FrameSize) {
            const auto begin = std::chrono::steady_clock::now();
            NoiseFrameResult frameResult = suppressor.ProcessFrame(
                m_InputFrame.data(), m_AlgorithmOutput.data(), m_FrameSize);
            frameResult.processMilliseconds = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - begin).count();
            if (!frameResult.valid) {
                std::copy(m_InputFrame.begin(), m_InputFrame.end(), m_AlgorithmOutput.begin());
            }
            double inputEnergy = 0.0;
            double outputEnergy = 0.0;
            for (std::size_t sample = 0; sample < m_FrameSize; ++sample) {
                inputEnergy += static_cast<double>(m_InputFrame[sample]) * m_InputFrame[sample];
                outputEnergy += static_cast<double>(m_AlgorithmOutput[sample]) *
                    m_AlgorithmOutput[sample];
            }
            frameResult.attenuationDb = static_cast<float>(std::clamp(
                10.0 * std::log10((inputEnergy + 1.0e-12) / (outputEnergy + 1.0e-12)),
                -60.0, 60.0));
            latest.voiceProbability = frameResult.voiceProbability;
            latest.valid = latest.valid && frameResult.valid;
            latest.processMilliseconds += frameResult.processMilliseconds;
            latest.maximumProcessMilliseconds = std::max(
                latest.maximumProcessMilliseconds, frameResult.processMilliseconds);
            attenuationSumDb += frameResult.attenuationDb;
            ++latest.algorithmFrames;
            PushProcessed(m_AlgorithmOutput);
            m_InputCount = 0;
            ++m_AlgorithmFrames;
        }
    }
    if (latest.algorithmFrames > 0) {
        latest.attenuationDb = static_cast<float>(
            attenuationSumDb / static_cast<double>(latest.algorithmFrames));
    }
    return latest;
}

NoiseFrameAdapterMetrics NoiseFrameAdapter::CaptureMetrics() const noexcept {
    return {m_InputFrames, m_OutputFrames, m_AlgorithmFrames,
        static_cast<std::uint32_t>(m_InputCount),
        static_cast<std::uint32_t>(m_ProcessedCount),
        static_cast<std::uint32_t>(m_FrameSize)};
}

const char* ToString(NoiseSuppressorMode mode) noexcept {
    switch (mode) {
    case NoiseSuppressorMode::Disabled: return "Disabled";
    case NoiseSuppressorMode::RNNoise: return "RNNoise";
    }
    return "Disabled";
}

const char* ToString(NoiseSuppressorState state) noexcept {
    switch (state) {
    case NoiseSuppressorState::Disabled: return "Disabled";
    case NoiseSuppressorState::Bypassed: return "Bypassed";
    case NoiseSuppressorState::Active: return "Active";
    case NoiseSuppressorState::UnsupportedFormat: return "Unsupported Format";
    case NoiseSuppressorState::InitializationFailed: return "Initialization Failed";
    case NoiseSuppressorState::Error: return "Error";
    }
    return "Error";
}

} // namespace Engine::Audio
