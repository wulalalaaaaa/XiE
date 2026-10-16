#pragma once

#include "Audio/DSP/BasicProcessors.h"

#include <array>
#include <atomic>
#include <complex>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace Engine::Audio {

using EQBandId = std::uint32_t;

inline constexpr std::size_t kParametricEQMaxBands = 16;
inline constexpr std::size_t kParametricEQMaxChannels = 8;
inline constexpr float kParametricEQMinimumFrequencyHz = 20.0f;
inline constexpr float kParametricEQMaximumFrequencyHz = 20'000.0f;
inline constexpr float kParametricEQMinimumGainDb = -24.0f;
inline constexpr float kParametricEQMaximumGainDb = 24.0f;
inline constexpr float kParametricEQMinimumQ = 0.1f;
inline constexpr float kParametricEQMaximumQ = 24.0f;
inline constexpr float kParametricEQDefaultQ = 0.70710678f;
inline constexpr float kParametricEQMinimumSlopeDbPerOctave = 6.0f;
inline constexpr float kParametricEQMaximumSlopeDbPerOctave = 24.0f;

enum class EQBandType : std::uint8_t {
    Bell,
    LowShelf,
    HighShelf,
    LowCut,
    HighCut,
    Notch
};

struct ParametricEQBand {
    EQBandId id = 0;
    bool enabled = true;
    EQBandType type = EQBandType::Bell;
    float frequencyHz = 1'000.0f;
    float gainDb = 0.0f;
    float q = kParametricEQDefaultQ;
    float slopeDbPerOctave = 12.0f;
};

[[nodiscard]] ParametricEQBand ClampParametricEQBand(
    ParametricEQBand band,
    float sampleRate = 48'000.0f) noexcept;

class ParametricEQModel {
public:
    [[nodiscard]] const std::vector<ParametricEQBand>& Bands() const noexcept { return m_Bands; }
    [[nodiscard]] std::optional<EQBandId> AddBand(ParametricEQBand band);
    bool UpdateBand(ParametricEQBand band);
    bool RemoveBand(EQBandId id) noexcept;
    void Reset() noexcept { m_Bands.clear(); }
    [[nodiscard]] ParametricEQBand* FindBand(EQBandId id) noexcept;
    [[nodiscard]] const ParametricEQBand* FindBand(EQBandId id) const noexcept;
    [[nodiscard]] bool Enabled() const noexcept { return m_Enabled; }
    void SetEnabled(bool enabled) noexcept { m_Enabled = enabled; }
    [[nodiscard]] EQBandId NextId() const noexcept;

private:
    std::vector<ParametricEQBand> m_Bands;
    bool m_Enabled = true;
};

struct BiquadCoefficients {
    double b0 = 1.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a1 = 0.0;
    double a2 = 0.0;

    [[nodiscard]] static constexpr BiquadCoefficients Identity() noexcept { return {}; }
};

[[nodiscard]] BiquadCoefficients CalculateParametricEQCoefficients(
    const ParametricEQBand& band,
    double sampleRate) noexcept;

struct ParametricEQSnapshot {
    bool enabled = true;
    double sampleRate = 48'000.0;
    std::uint64_t revision = 0;
    std::size_t bandCount = 0;
    std::array<ParametricEQBand, kParametricEQMaxBands> bands{};
    std::array<BiquadCoefficients, kParametricEQMaxBands> coefficients{};
};

[[nodiscard]] std::shared_ptr<const ParametricEQSnapshot> BuildParametricEQSnapshot(
    const ParametricEQModel& model,
    double sampleRate,
    std::uint64_t revision);

struct EQResponseData {
    std::vector<float> frequenciesHz;
    std::vector<std::vector<float>> bandResponseDb;
    std::vector<float> combinedResponseDb;
};

class EQResponseCalculator {
public:
    [[nodiscard]] static EQResponseData Calculate(
        const ParametricEQModel& model,
        double sampleRate,
        std::size_t sampleCount = 1024,
        float minimumFrequencyHz = kParametricEQMinimumFrequencyHz,
        float maximumFrequencyHz = kParametricEQMaximumFrequencyHz);
    [[nodiscard]] static float MagnitudeDb(
        const BiquadCoefficients& coefficients,
        double sampleRate,
        double frequencyHz) noexcept;
};

struct ParametricEQMetrics {
    bool enabled = true;
    std::size_t bandCount = 0;
    std::uint64_t snapshotRevision = 0;
    double processMilliseconds = 0.0;
    double maximumProcessMilliseconds = 0.0;
};

class ParametricEQProcessor final : public ProcessorBase {
public:
    AudioStatus Prepare(const AudioFormat& format) override;
    void Process(AudioProcessContext& context) noexcept override;
    void Reset() noexcept override;
    [[nodiscard]] const char* Name() const noexcept override { return "ParametricEQ"; }

    AudioStatus Publish(std::shared_ptr<const ParametricEQSnapshot> snapshot);
    [[nodiscard]] ParametricEQMetrics CaptureMetrics() const noexcept;

private:
    struct FilterState {
        BiquadCoefficients current{};
        BiquadCoefficients target{};
        BiquadCoefficients step{};
        std::uint32_t remaining = 0;
        std::array<double, kParametricEQMaxChannels> z1{};
        std::array<double, kParametricEQMaxChannels> z2{};
    };

    void AdoptSnapshot(const ParametricEQSnapshot* snapshot) noexcept;
    static void AdvanceCoefficients(FilterState& state) noexcept;

    AudioFormat m_Format{};
    std::array<FilterState, kParametricEQMaxBands> m_Filters{};
    std::vector<std::shared_ptr<const ParametricEQSnapshot>> m_Retained;
    std::atomic<const ParametricEQSnapshot*> m_Current{nullptr};
    const ParametricEQSnapshot* m_AudioSnapshot = nullptr;
    std::uint32_t m_SmoothingFrames = 64;
    bool m_Prepared = false;
    std::atomic<bool> m_Enabled{true};
    std::atomic<std::size_t> m_BandCount{0};
    std::atomic<std::uint64_t> m_Revision{0};
    std::atomic<double> m_ProcessMilliseconds{0.0};
    std::atomic<double> m_MaximumProcessMilliseconds{0.0};
};

[[nodiscard]] const char* ToString(EQBandType type) noexcept;

} // namespace Engine::Audio
