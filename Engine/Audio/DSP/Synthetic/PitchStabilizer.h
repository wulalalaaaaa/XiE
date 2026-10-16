#pragma once

#include "Audio/DSP/Pitch.h"

#include <atomic>
#include <cstdint>

namespace Engine::Audio {

struct PitchStabilizerConfig {
    bool enabled = false;
    float stability = 0.0f;
    float response = 0.65f;
    float maxCorrectionCents = 12.0f;
    float minimumConfidence = 0.65f;
    float holdMilliseconds = 80.0f;
};

[[nodiscard]] PitchStabilizerConfig ClampPitchStabilizerConfig(
    PitchStabilizerConfig config) noexcept;

struct PitchStabilizerResult {
    float rawFrequencyHz = 0.0f;
    float stabilizedFrequencyHz = 0.0f;
    float correctionCents = 0.0f;
    float confidence = 0.0f;
    bool voiced = false;
    bool held = false;
};

// A deliberately conservative control-rate stabilizer. It never snaps to a
// scale and only produces a bounded correction around the detected F0. Large
// movements are followed quickly, while small movements are low-passed.
class PitchStabilizer final {
public:
    void Prepare(const AudioFormat& format) noexcept;
    void SetConfig(PitchStabilizerConfig config) noexcept;
    [[nodiscard]] PitchStabilizerResult Process(
        PitchEstimate estimate,
        std::uint32_t frameCount) noexcept;
    void Reset() noexcept;

    [[nodiscard]] PitchStabilizerResult Latest() const noexcept { return m_Latest; }
    [[nodiscard]] std::uint32_t HoldFrames() const noexcept { return m_HoldFrames; }
    [[nodiscard]] float ControlSmoothingMilliseconds() const noexcept {
        return m_ControlSmoothingMilliseconds.load(std::memory_order_acquire);
    }

private:
    AudioFormat m_Format{};
    PitchStabilizerConfig m_Config{};
    PitchStabilizerResult m_Latest{};
    float m_StabilizedFrequencyHz = 0.0f;
    bool m_HasFrequency = false;
    std::uint32_t m_HoldFrames = 0;
    std::uint32_t m_InvalidFrames = 0;
    std::atomic<float> m_ControlSmoothingMilliseconds{80.0f};
};

} // namespace Engine::Audio
