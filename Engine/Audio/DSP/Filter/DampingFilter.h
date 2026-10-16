#pragma once

#include "Audio/Core/AudioTypes.h"

#include <cstddef>
#include <vector>

namespace Engine::Audio {

// One-pole low-pass used as a stable high-frequency damping stage. Damping
// is normalized to [0, 1], where 0 leaves the signal unchanged.
class DampingFilter final {
public:
    AudioStatus Prepare(const AudioFormat& format);
    void SetDamping(float damping) noexcept;
    [[nodiscard]] float ProcessSample(std::size_t channel, float input) noexcept;
    void Reset() noexcept;
    [[nodiscard]] std::size_t MemoryBytes() const noexcept { return m_State.size() * sizeof(float); }

private:
    std::vector<float> m_State;
    float m_Damping = 0.0f;
};

} // namespace Engine::Audio
