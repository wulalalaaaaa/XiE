#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace Engine::Audio {

enum class AudioParameterId : std::uint8_t {
    Wet,
    Gain,
    PitchSemitones,
    FormantSemitones,
    OutputGain,
    Bypass,
    Count
};

inline constexpr std::size_t kAudioParameterCount =
    static_cast<std::size_t>(AudioParameterId::Count);

class AudioParameter {
public:
    explicit AudioParameter(float initial = 0.0f) noexcept : m_Value(initial) {}
    void Store(float value) noexcept { m_Value.store(value, std::memory_order_release); }
    [[nodiscard]] float Load() const noexcept { return m_Value.load(std::memory_order_acquire); }

private:
    std::atomic<float> m_Value;
};

struct AudioParameterSnapshot {
    std::array<float, kAudioParameterCount> values{};
    std::uint64_t generation = 0;

    [[nodiscard]] float Get(AudioParameterId id) const noexcept {
        return values[static_cast<std::size_t>(id)];
    }
};

class AudioParameterStore {
public:
    AudioParameterStore() noexcept {
        for (auto& value : m_Values) {
            value.store(0.0f, std::memory_order_relaxed);
        }
    }

    void Store(AudioParameterId id, float value) noexcept {
        m_Values[static_cast<std::size_t>(id)].store(value, std::memory_order_relaxed);
        m_Generation.fetch_add(1, std::memory_order_release);
    }

    [[nodiscard]] AudioParameterSnapshot Capture() const noexcept {
        AudioParameterSnapshot snapshot;
        // A bounded single pass is intentional: the audio thread never waits
        // for a writer. Each parameter is atomic; generation identifies the
        // newest publication observed by this callback.
        snapshot.generation = m_Generation.load(std::memory_order_acquire);
        for (std::size_t i = 0; i < kAudioParameterCount; ++i) {
            snapshot.values[i] = m_Values[i].load(std::memory_order_acquire);
        }
        return snapshot;
    }

private:
    std::array<std::atomic<float>, kAudioParameterCount> m_Values;
    mutable std::atomic<std::uint64_t> m_Generation{0};
};

class ParameterSmoother {
public:
    void Prepare(double sampleRate, double rampMilliseconds, float initialValue) noexcept {
        m_Current = initialValue;
        m_Target = initialValue;
        m_Remaining = 0;
        m_RampSamples = std::max<std::uint32_t>(
            1,
            static_cast<std::uint32_t>(sampleRate * rampMilliseconds / 1000.0));
    }

    void SetTarget(float target) noexcept {
        if (target == m_Target) {
            return;
        }
        m_Target = target;
        m_Remaining = m_RampSamples;
        m_Step = (m_Target - m_Current) / static_cast<float>(m_Remaining);
    }

    [[nodiscard]] float Next() noexcept {
        if (m_Remaining > 0) {
            m_Current += m_Step;
            if (--m_Remaining == 0) {
                m_Current = m_Target;
            }
        }
        return m_Current;
    }

    void Reset(float value) noexcept {
        m_Current = value;
        m_Target = value;
        m_Remaining = 0;
        m_Step = 0.0f;
    }

    [[nodiscard]] float Current() const noexcept { return m_Current; }

private:
    float m_Current = 0.0f;
    float m_Target = 0.0f;
    float m_Step = 0.0f;
    std::uint32_t m_RampSamples = 1;
    std::uint32_t m_Remaining = 0;
};

} // namespace Engine::Audio
