#pragma once

#include "Audio/Backend/WASAPI/WasapiBackend.h"
#include "Audio/Core/AudioSpscRingBuffer.h"
#include "Audio/Voice/VoiceProcessor.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace XiE::VoiceMixer {

enum class VoiceMixerAudioState : std::uint8_t {
    Stopped,
    Starting,
    Running,
    Stopping,
    DeviceLost,
    Error
};

struct VoiceMixerParameters {
    float wet = 1.0f;
    float inputGain = 1.0f;
    float outputGain = 1.0f;
    float pitchSemitones = 0.0f;
    float formantSemitones = 0.0f;
    bool bypass = false;
    bool monitor = true;
    bool analyzerEnabled = true;
    bool noiseEnabled = false;
    bool noiseBypassed = false;
    float noiseStrength = 1.0f;
    float noiseWet = 1.0f;
    Engine::Audio::SoftHazeConfig softHaze{};
    Engine::Audio::SpatialHazeConfig spatialHaze{};
    Engine::Audio::SyntheticVoiceConfig syntheticVoice{};
    Engine::Audio::ElectronicMistConfig electronicMist{};
};

struct VoiceMixerAudioSnapshot {
    VoiceMixerAudioState state = VoiceMixerAudioState::Stopped;
    Engine::Audio::AudioStatus lastStatus{};
    Engine::Audio::AudioFormat format{};
    Engine::Audio::VoiceProcessorMetrics voice{};
    Engine::Audio::AudioMetricsSnapshot diagnostics{};
    float inputPeak = 0.0f;
    float outputPeak = 0.0f;
    double deadlinePercent = 0.0;
    std::uint64_t underruns = 0;
    std::uint64_t rtAllocationsObserved = 0;
    std::shared_ptr<const Engine::Audio::SpectrumSnapshot> spectrum;
};

class VoiceMixerAudioController {
public:
    VoiceMixerAudioController();
    explicit VoiceMixerAudioController(std::unique_ptr<Engine::Audio::IAudioBackend> backend);
    ~VoiceMixerAudioController();

    VoiceMixerAudioController(const VoiceMixerAudioController&) = delete;
    VoiceMixerAudioController& operator=(const VoiceMixerAudioController&) = delete;

    Engine::Audio::AudioStatus RefreshDevices();
    [[nodiscard]] const std::vector<Engine::Audio::AudioDeviceInfo>& InputDevices() const noexcept {
        return m_InputDevices;
    }
    [[nodiscard]] const std::vector<Engine::Audio::AudioDeviceInfo>& OutputDevices() const noexcept {
        return m_OutputDevices;
    }
    bool SelectInput(std::size_t index) noexcept;
    bool SelectOutput(std::size_t index) noexcept;
    [[nodiscard]] std::size_t SelectedInput() const noexcept { return m_SelectedInput; }
    [[nodiscard]] std::size_t SelectedOutput() const noexcept { return m_SelectedOutput; }

    Engine::Audio::AudioStatus Start();
    Engine::Audio::AudioStatus Stop();
    Engine::Audio::AudioStatus Restart();
    void Poll() noexcept;

    // True while the selected endpoints look like a desktop/virtual loopback
    // feeding a virtual output. In that route Monitor is forced off to avoid
    // sending the processed signal back into the capture stream.
    [[nodiscard]] bool MonitorSafetyActive() const noexcept {
        return m_MonitorSafetyActive.load(std::memory_order_acquire);
    }

    void ApplyParameters(const VoiceMixerParameters& parameters) noexcept;
    Engine::Audio::AudioStatus ApplyCurveSnapshots(
        std::shared_ptr<const Engine::Curve::CurveSnapshot> pitch,
        std::shared_ptr<const Engine::Curve::CurveSnapshot> formant,
        std::shared_ptr<const Engine::Curve::CurveSnapshot> electronicNoise = {});
    Engine::Audio::AudioStatus ApplyParametricEQ(
        const Engine::Audio::ParametricEQModel& model);

    [[nodiscard]] VoiceMixerAudioSnapshot CaptureSnapshot() const noexcept;
    [[nodiscard]] VoiceMixerAudioState State() const noexcept {
        return m_State.load(std::memory_order_acquire);
    }

private:
    static void CaptureCallback(
        void* userData,
        Engine::Audio::ConstAudioBufferView input,
        std::uint64_t streamFrame) noexcept;
    static void RenderCallback(
        void* userData,
        Engine::Audio::AudioBufferView output,
        std::uint64_t streamFrame) noexcept;
    void SetFailure(Engine::Audio::AudioStatus status) noexcept;

    std::unique_ptr<Engine::Audio::IAudioBackend> m_Backend;
    std::vector<Engine::Audio::AudioDeviceInfo> m_InputDevices;
    std::vector<Engine::Audio::AudioDeviceInfo> m_OutputDevices;
    std::size_t m_SelectedInput = 0;
    std::size_t m_SelectedOutput = 0;
    std::unique_ptr<Engine::Audio::IAudioInputStream> m_Input;
    std::unique_ptr<Engine::Audio::IAudioOutputStream> m_Output;
    Engine::Audio::AudioSpscRingBuffer m_Ring;
    Engine::Audio::AudioBuffer m_ProcessBuffer;
    Engine::Audio::VoiceProcessor m_Voice;
    Engine::Audio::AudioDiagnostics m_Diagnostics;
    std::atomic<VoiceMixerAudioState> m_State{VoiceMixerAudioState::Stopped};
    std::atomic<Engine::Audio::AudioStatusCode> m_StatusCode{Engine::Audio::AudioStatusCode::Ok};
    std::atomic<std::int64_t> m_PlatformCode{0};
    std::atomic<float> m_InputPeak{0.0f};
    std::atomic<bool> m_Monitor{true};
    std::atomic<bool> m_MonitorSafetyActive{false};
    std::atomic<bool> m_OutputPrimed{false};
    std::atomic<std::uint64_t> m_Underruns{0};
    std::atomic<std::uint64_t> m_RtAllocations{0};
    Engine::Audio::AudioFormat m_Format{};
    VoiceMixerParameters m_Parameters{};
    Engine::Audio::ParametricEQModel m_EQModel;
    std::uint64_t m_EQRevision = 0;
    Engine::Audio::NoiseSuppressorConfig m_NoiseConfig{};
    std::uint64_t m_NoiseRevision = 0;
    Engine::Audio::SoftHazeConfig m_SoftHazeConfig{};
    Engine::Audio::SpatialHazeConfig m_SpatialHazeConfig{};
    std::uint64_t m_SoftHazeRevision = 0;
    std::uint64_t m_SpatialHazeRevision = 0;
    Engine::Audio::SyntheticVoiceConfig m_SyntheticVoiceConfig{};
    std::uint64_t m_SyntheticVoiceRevision = 0;
    Engine::Audio::ElectronicMistConfig m_ElectronicMistConfig{};
    std::shared_ptr<const Engine::Curve::CurveSnapshot> m_ElectronicNoiseCurve;
    std::uint64_t m_ElectronicMistRevision = 0;
};

[[nodiscard]] const char* ToString(VoiceMixerAudioState state) noexcept;

} // namespace XiE::VoiceMixer
