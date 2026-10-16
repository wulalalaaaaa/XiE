#pragma once

#include "Audio/DSP/BasicProcessors.h"
#include "Curve/Curve.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace Engine::Audio {

struct PitchEstimate {
    float frequencyHz = 0.0f;
    float confidence = 0.0f;
    bool voiced = false;
};

class IPitchDetector {
public:
    virtual ~IPitchDetector() = default;
    virtual AudioStatus Prepare(const AudioFormat& format) = 0;
    virtual PitchEstimate Process(ConstAudioBufferView input) noexcept = 0;
    virtual void Reset() noexcept = 0;
    [[nodiscard]] virtual PitchEstimate Latest() const noexcept = 0;
};

class PitchDetector final : public IPitchDetector {
public:
    struct Config {
        float minimumFrequencyHz = 70.0f;
        float maximumFrequencyHz = 500.0f;
        float yinThreshold = 0.15f;
        float minimumRms = 0.002f;
        std::uint32_t windowFrames = 1536;
        std::uint32_t analysisHopFrames = 128;
    };

    PitchDetector() = default;
    explicit PitchDetector(Config config) : m_Config(config) {}

    void SetConfig(Config config) noexcept { m_Config = config; }
    AudioStatus Prepare(const AudioFormat& format) override;
    PitchEstimate Process(ConstAudioBufferView input) noexcept override;
    void Reset() noexcept override;
    [[nodiscard]] PitchEstimate Latest() const noexcept override { return m_Latest; }

private:
    void Analyze() noexcept;

    Config m_Config{};
    AudioFormat m_Format{};
    std::vector<float> m_Ring;
    std::vector<float> m_Window;
    std::vector<float> m_Difference;
    std::size_t m_WriteIndex = 0;
    std::size_t m_ValidFrames = 0;
    std::uint32_t m_FramesSinceAnalysis = 0;
    std::uint32_t m_MinimumLag = 1;
    std::uint32_t m_MaximumLag = 1;
    PitchEstimate m_Latest{};
};

class PitchCurve {
public:
    AudioStatus Publish(std::shared_ptr<const Curve::CurveSnapshot> snapshot);
    [[nodiscard]] float Evaluate(float frequencyHz, float fallbackSemitones = 0.0f) const noexcept;
    [[nodiscard]] static float HertzToMidi(float frequencyHz) noexcept;
    [[nodiscard]] static float MidiToHertz(float midiNote) noexcept;

private:
    std::vector<std::shared_ptr<const Curve::CurveSnapshot>> m_Retained;
    std::atomic<const Curve::CurveSnapshot*> m_Current{nullptr};
};

class IPitchShifter {
public:
    virtual ~IPitchShifter() = default;
    virtual AudioStatus PreparePitch(const AudioFormat& format) = 0;
    virtual void SetPitchSemitones(float semitones) noexcept = 0;
    virtual void ProcessPitch(AudioProcessContext& context) noexcept = 0;
    virtual void ResetPitch() noexcept = 0;
    [[nodiscard]] virtual std::uint32_t LatencyFrames() const noexcept = 0;
};

class PitchShiftProcessor final : public ProcessorBase, public IPitchShifter {
public:
    explicit PitchShiftProcessor(std::uint32_t windowFrames = 512) noexcept
        : m_WindowFrames(windowFrames) {}

    void SetWindowFrames(std::uint32_t windowFrames) noexcept { m_WindowFrames = windowFrames; }
    AudioStatus Prepare(const AudioFormat& format) override { return PreparePitch(format); }
    AudioStatus PreparePitch(const AudioFormat& format) override;
    void SetPitchSemitones(float semitones) noexcept override;
    void Process(AudioProcessContext& context) noexcept override { ProcessPitch(context); }
    void ProcessPitch(AudioProcessContext& context) noexcept override;
    void Reset() noexcept override { ResetPitch(); }
    void ResetPitch() noexcept override;
    [[nodiscard]] std::uint32_t LatencyFrames() const noexcept override {
        return m_WindowFrames / 2;
    }
    [[nodiscard]] const char* Name() const noexcept override { return "PitchShift"; }

private:
    [[nodiscard]] float Read(std::size_t channel, float delayFrames) const noexcept;

    std::uint32_t m_WindowFrames = 512;
    std::size_t m_Channels = 0;
    std::size_t m_Capacity = 0;
    std::size_t m_WriteIndex = 0;
    float m_Phase = 0.25f;
    std::vector<float> m_Delay;
    std::atomic<float> m_TargetSemitones{0.0f};
    ParameterSmoother m_RatioSmoother;
};

} // namespace Engine::Audio
