#pragma once

#include "Audio/Core/AudioParameter.h"
#include "Audio/Graph/AudioProcessor.h"

#include <atomic>
#include <cstddef>
#include <vector>

namespace Engine::Audio {

class ProcessorBase : public IAudioProcessor {
public:
    void SetBypassed(bool bypassed) noexcept override {
        m_Bypassed.store(bypassed, std::memory_order_release);
    }
    [[nodiscard]] bool Bypassed() const noexcept override {
        return m_Bypassed.load(std::memory_order_acquire);
    }

private:
    std::atomic<bool> m_Bypassed{false};
};

class GainProcessor final : public ProcessorBase {
public:
    explicit GainProcessor(float gain = 1.0f) noexcept : m_TargetGain(gain) {}
    AudioStatus Prepare(const AudioFormat& format) override;
    void Process(AudioProcessContext& context) noexcept override;
    void Reset() noexcept override;
    void SetGain(float gain) noexcept { m_TargetGain.store(gain, std::memory_order_release); }
    [[nodiscard]] const char* Name() const noexcept override { return "Gain"; }

private:
    std::atomic<float> m_TargetGain{1.0f};
    ParameterSmoother m_Smoother;
    double m_SampleRate = 48'000.0;
};

class BypassProcessor final : public ProcessorBase {
public:
    AudioStatus Prepare(const AudioFormat& format) override;
    void Process(AudioProcessContext& context) noexcept override;
    void Reset() noexcept override {}
    [[nodiscard]] const char* Name() const noexcept override { return "Bypass"; }
};

class ChannelMixer final : public ProcessorBase {
public:
    // Row-major outputChannel x inputChannel matrix. Configure before Prepare.
    void SetMatrix(std::size_t channels, std::vector<float> gains);
    AudioStatus Prepare(const AudioFormat& format) override;
    void Process(AudioProcessContext& context) noexcept override;
    void Reset() noexcept override;
    [[nodiscard]] const char* Name() const noexcept override { return "ChannelMixer"; }

private:
    std::size_t m_Channels = 0;
    std::size_t m_MaxFrames = 0;
    std::vector<float> m_Matrix;
    std::vector<float> m_Scratch;
};

class DCBlocker final : public ProcessorBase {
public:
    explicit DCBlocker(float cutoffHz = 20.0f) noexcept : m_CutoffHz(cutoffHz) {}
    AudioStatus Prepare(const AudioFormat& format) override;
    void Process(AudioProcessContext& context) noexcept override;
    void Reset() noexcept override;
    [[nodiscard]] const char* Name() const noexcept override { return "DCBlocker"; }

private:
    float m_CutoffHz = 20.0f;
    float m_R = 0.995f;
    std::vector<float> m_PreviousInput;
    std::vector<float> m_PreviousOutput;
};

class Limiter final : public ProcessorBase {
public:
    Limiter(float ceiling = 0.98f, float releaseMilliseconds = 50.0f) noexcept
        : m_Ceiling(ceiling), m_ReleaseMilliseconds(releaseMilliseconds) {}
    AudioStatus Prepare(const AudioFormat& format) override;
    void Process(AudioProcessContext& context) noexcept override;
    void Reset() noexcept override;
    [[nodiscard]] const char* Name() const noexcept override { return "Limiter"; }

private:
    float m_Ceiling = 0.98f;
    float m_ReleaseMilliseconds = 50.0f;
    float m_ReleaseCoefficient = 0.999f;
    float m_Gain = 1.0f;
};

class MeterProcessor final : public ProcessorBase {
public:
    AudioStatus Prepare(const AudioFormat& format) override;
    void Process(AudioProcessContext& context) noexcept override;
    void Reset() noexcept override;
    [[nodiscard]] float Peak() const noexcept { return m_Peak.load(std::memory_order_acquire); }
    [[nodiscard]] float Rms() const noexcept { return m_Rms.load(std::memory_order_acquire); }
    [[nodiscard]] const char* Name() const noexcept override { return "PeakRMSMeter"; }

private:
    std::atomic<float> m_Peak{0.0f};
    std::atomic<float> m_Rms{0.0f};
};

} // namespace Engine::Audio
