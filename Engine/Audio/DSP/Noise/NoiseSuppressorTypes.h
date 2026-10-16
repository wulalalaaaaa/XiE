#pragma once

#include "Audio/Core/AudioTypes.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace Engine::Audio {

enum class NoiseSuppressorMode : std::uint8_t {
    Disabled,
    RNNoise
};

enum class NoiseSuppressorState : std::uint8_t {
    Disabled,
    Bypassed,
    Active,
    UnsupportedFormat,
    InitializationFailed,
    Error
};

struct NoiseSuppressorConfig {
    bool enabled = false;
    NoiseSuppressorMode mode = NoiseSuppressorMode::RNNoise;
    float strength = 1.0f;
    float wet = 1.0f;
};

[[nodiscard]] inline NoiseSuppressorConfig ClampNoiseSuppressorConfig(
    NoiseSuppressorConfig config) noexcept {
    config.strength = std::clamp(
        std::isfinite(config.strength) ? config.strength : 1.0f, 0.0f, 1.0f);
    config.wet = std::clamp(
        std::isfinite(config.wet) ? config.wet : 1.0f, 0.0f, 1.0f);
    return config;
}

struct NoiseSuppressorPrepareInfo {
    AudioFormat streamFormat{};
};

struct NoiseFrameResult {
    float voiceProbability = 0.0f;
    float attenuationDb = 0.0f;
    double processMilliseconds = 0.0;
    bool valid = true;
    std::uint32_t algorithmFrames = 0;
    double maximumProcessMilliseconds = 0.0;
};

struct NoiseSuppressorSnapshot {
    NoiseSuppressorConfig config{};
    std::uint64_t revision = 0;
};

[[nodiscard]] inline std::shared_ptr<const NoiseSuppressorSnapshot>
BuildNoiseSuppressorSnapshot(NoiseSuppressorConfig config, std::uint64_t revision) {
    auto snapshot = std::make_shared<NoiseSuppressorSnapshot>();
    snapshot->config = ClampNoiseSuppressorConfig(config);
    snapshot->revision = revision;
    return snapshot;
}

struct NoiseSuppressorMetrics {
    NoiseSuppressorState state = NoiseSuppressorState::Disabled;
    NoiseSuppressorMode mode = NoiseSuppressorMode::RNNoise;
    bool enabled = false;
    bool bypassed = false;
    bool active = false;
    float strength = 1.0f;
    float wet = 1.0f;
    float voiceProbability = 0.0f;
    float averageAttenuationDb = 0.0f;
    float peakAttenuationDb = 0.0f;
    std::uint64_t processedFrames = 0;
    std::uint64_t algorithmFrames = 0;
    std::uint64_t droppedFrames = 0;
    std::uint64_t snapshotRevision = 0;
    double averageProcessMilliseconds = 0.0;
    double p95ProcessMilliseconds = 0.0;
    double p99ProcessMilliseconds = 0.0;
    double maximumProcessMilliseconds = 0.0;
    std::uint32_t frameSize = 0;
    std::uint32_t bufferedInputFrames = 0;
    std::uint32_t bufferedOutputFrames = 0;
    double algorithmicLatencyMilliseconds = 0.0;
    std::uint64_t rtAllocations = 0;
};

[[nodiscard]] const char* ToString(NoiseSuppressorMode mode) noexcept;
[[nodiscard]] const char* ToString(NoiseSuppressorState state) noexcept;

} // namespace Engine::Audio
