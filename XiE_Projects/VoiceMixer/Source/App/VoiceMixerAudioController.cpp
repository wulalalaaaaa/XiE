#include "App/VoiceMixerAudioController.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cwctype>
#include <string_view>

namespace XiE::VoiceMixer {
namespace Audio = Engine::Audio;

namespace {

bool ContainsInsensitive(std::wstring_view value, std::wstring_view needle) noexcept {
    if (needle.empty() || value.size() < needle.size()) return false;
    return std::search(value.begin(), value.end(), needle.begin(), needle.end(),
        [](wchar_t left, wchar_t right) {
            return std::towlower(left) == std::towlower(right);
        }) != value.end();
}

bool IsPotentialMonitorFeedbackRoute(
    const Audio::AudioDeviceInfo& input,
    const Audio::AudioDeviceInfo& output) noexcept {
    const bool loopbackInput =
        ContainsInsensitive(input.name, L"voicemeeter output") ||
        ContainsInsensitive(input.name, L"cable output") ||
        ContainsInsensitive(input.name, L"stereo mix") ||
        ContainsInsensitive(input.name, L"what u hear") ||
        ContainsInsensitive(input.name, L"loopback");
    const bool virtualOutput =
        ContainsInsensitive(output.name, L"voicemeeter input") ||
        ContainsInsensitive(output.name, L"cable input") ||
        ContainsInsensitive(output.name, L"virtual input");
    return loopbackInput && virtualOutput;
}

} // namespace

VoiceMixerAudioController::VoiceMixerAudioController()
    : VoiceMixerAudioController(std::make_unique<Audio::WasapiBackend>()) {}

VoiceMixerAudioController::VoiceMixerAudioController(
    std::unique_ptr<Audio::IAudioBackend> backend)
    : m_Backend(std::move(backend)) {}

VoiceMixerAudioController::~VoiceMixerAudioController() {
    (void)Stop();
}

Audio::AudioStatus VoiceMixerAudioController::RefreshDevices() {
    if (State() == VoiceMixerAudioState::Running ||
        State() == VoiceMixerAudioState::Starting) {
        return {Audio::AudioStatusCode::InvalidArgument, 0};
    }
    if (State() == VoiceMixerAudioState::DeviceLost ||
        State() == VoiceMixerAudioState::Error) {
        (void)Stop();
        m_State.store(VoiceMixerAudioState::Stopped, std::memory_order_release);
    }
    const Audio::AudioDeviceId previousInput =
        m_SelectedInput < m_InputDevices.size() ? m_InputDevices[m_SelectedInput].id : L"";
    const Audio::AudioDeviceId previousOutput =
        m_SelectedOutput < m_OutputDevices.size() ? m_OutputDevices[m_SelectedOutput].id : L"";
    Audio::AudioStatus status;
    auto inputs = m_Backend->EnumerateDevices(Audio::AudioDeviceDirection::Input, &status);
    if (!status) {
        SetFailure(status);
        return status;
    }
    auto outputs = m_Backend->EnumerateDevices(Audio::AudioDeviceDirection::Output, &status);
    if (!status) {
        SetFailure(status);
        return status;
    }
    m_InputDevices = std::move(inputs);
    m_OutputDevices = std::move(outputs);
    const auto selectEndpoint = [](const auto& devices, const Audio::AudioDeviceId& previous) {
        auto selected = std::find_if(devices.begin(), devices.end(), [&](const auto& device) {
            return !previous.empty() && device.id == previous;
        });
        if (selected == devices.end()) {
            selected = std::find_if(devices.begin(), devices.end(), [](const auto& device) {
                return device.isDefault;
            });
        }
        return selected == devices.end()
            ? std::size_t{0}
            : static_cast<std::size_t>(std::distance(devices.begin(), selected));
    };
    m_SelectedInput = selectEndpoint(m_InputDevices, previousInput);
    m_SelectedOutput = selectEndpoint(m_OutputDevices, previousOutput);
    m_StatusCode.store(Audio::AudioStatusCode::Ok, std::memory_order_release);
    m_PlatformCode.store(0, std::memory_order_release);
    if (State() == VoiceMixerAudioState::Error ||
        State() == VoiceMixerAudioState::DeviceLost) {
        m_State.store(VoiceMixerAudioState::Stopped, std::memory_order_release);
    }
    return {};
}

bool VoiceMixerAudioController::SelectInput(std::size_t index) noexcept {
    if (State() != VoiceMixerAudioState::Stopped || index >= m_InputDevices.size()) {
        return false;
    }
    m_SelectedInput = index;
    return true;
}

bool VoiceMixerAudioController::SelectOutput(std::size_t index) noexcept {
    if (State() != VoiceMixerAudioState::Stopped || index >= m_OutputDevices.size()) {
        return false;
    }
    m_SelectedOutput = index;
    return true;
}

Audio::AudioStatus VoiceMixerAudioController::Start() {
    if (State() != VoiceMixerAudioState::Stopped) {
        return {Audio::AudioStatusCode::InvalidArgument, 0};
    }
    if (m_InputDevices.empty() || m_OutputDevices.empty() ||
        m_SelectedInput >= m_InputDevices.size() || m_SelectedOutput >= m_OutputDevices.size()) {
        const Audio::AudioStatus status{Audio::AudioStatusCode::DeviceNotFound, 0};
        SetFailure(status);
        return status;
    }
    m_State.store(VoiceMixerAudioState::Starting, std::memory_order_release);

    Audio::AudioStreamConfig inputConfig;
    inputConfig.deviceId = m_InputDevices[m_SelectedInput].id;
    inputConfig.requestedFormat = {48'000, 1, Audio::AudioSampleFormat::Float32, 128};
    Audio::AudioStreamConfig outputConfig;
    outputConfig.deviceId = m_OutputDevices[m_SelectedOutput].id;
    outputConfig.requestedFormat = inputConfig.requestedFormat;

    Audio::AudioStatus status;
    m_Input = m_Backend->OpenInput(inputConfig, &status);
    if (!m_Input || !status) {
        SetFailure(status);
        return status;
    }
    m_Output = m_Backend->OpenOutput(outputConfig, &status);
    if (!m_Output || !status) {
        m_Input.reset();
        SetFailure(status);
        return status;
    }
    m_Format = m_Input->Format();
    m_MonitorSafetyActive.store(IsPotentialMonitorFeedbackRoute(
        m_InputDevices[m_SelectedInput], m_OutputDevices[m_SelectedOutput]),
        std::memory_order_release);
    const std::uint32_t maximumFrames = std::max(
        m_Input->Format().framesPerBuffer, m_Output->Format().framesPerBuffer);
    m_Format.framesPerBuffer = maximumFrames;
    m_ProcessBuffer.Resize(m_Format.channelCount, maximumFrames);
    m_Ring.Prepare(m_Format.channelCount, m_Format.sampleRate / 2);
    Audio::VoiceProcessorConfig voiceConfig;
    voiceConfig.format = m_Format;
    voiceConfig.parametricEQ = m_EQModel;
    voiceConfig.softHaze = m_Parameters.softHaze;
    voiceConfig.spatialHaze = m_Parameters.spatialHaze;
    voiceConfig.syntheticVoice = m_Parameters.syntheticVoice;
    voiceConfig.electronicMist = m_Parameters.electronicMist;
    voiceConfig.noiseSuppression = {
        m_Parameters.noiseEnabled,
        Audio::NoiseSuppressorMode::RNNoise,
        m_Parameters.noiseStrength,
        m_Parameters.noiseWet};
    voiceConfig.analyzer.enabled = m_Parameters.analyzerEnabled;
    status = m_Voice.Prepare(voiceConfig);
    if (!status) {
        m_Input.reset();
        m_Output.reset();
        SetFailure(status);
        return status;
    }
    ApplyParameters(m_Parameters);
    (void)ApplyParametricEQ(m_EQModel);
    if (m_ElectronicNoiseCurve) {
        (void)m_Voice.PublishElectronicMist(Audio::BuildElectronicMistSnapshot(
            m_ElectronicMistConfig, m_ElectronicNoiseCurve, ++m_ElectronicMistRevision));
    }
    m_InputPeak.store(0.0f, std::memory_order_relaxed);
    m_OutputPrimed.store(false, std::memory_order_relaxed);
    m_Underruns.store(0, std::memory_order_relaxed);

    status = m_Output->Start(&RenderCallback, this);
    if (!status) {
        m_Input.reset();
        m_Output.reset();
        SetFailure(status);
        return status;
    }
    status = m_Input->Start(&CaptureCallback, this);
    if (!status) {
        (void)m_Output->Stop();
        m_Input.reset();
        m_Output.reset();
        SetFailure(status);
        return status;
    }

    Audio::AudioLatency latency;
    latency.captureMilliseconds = m_Input->Latency().captureMilliseconds;
    latency.processingMilliseconds =
        1000.0 * static_cast<double>(m_Voice.LatencyFrames()) / m_Format.sampleRate;
    latency.outputMilliseconds = m_Output->Latency().outputMilliseconds;
    m_Diagnostics.SetLatency(latency);
    m_StatusCode.store(Audio::AudioStatusCode::Ok, std::memory_order_release);
    m_PlatformCode.store(0, std::memory_order_release);
    m_State.store(VoiceMixerAudioState::Running, std::memory_order_release);
    return {};
}

Audio::AudioStatus VoiceMixerAudioController::Stop() {
    m_MonitorSafetyActive.store(false, std::memory_order_release);
    const VoiceMixerAudioState state = State();
    if (state == VoiceMixerAudioState::Stopped) {
        return {};
    }
    m_State.store(VoiceMixerAudioState::Stopping, std::memory_order_release);
    Audio::AudioStatus firstFailure;
    if (m_Input) {
        const Audio::AudioStatus status = m_Input->Stop();
        if (!status) firstFailure = status;
    }
    if (m_Output) {
        const Audio::AudioStatus status = m_Output->Stop();
        if (!status && firstFailure) firstFailure = status;
    }
    m_Input.reset();
    m_Output.reset();
    m_Ring.Reset();
    if (!firstFailure) {
        SetFailure(firstFailure);
        return firstFailure;
    }
    m_State.store(VoiceMixerAudioState::Stopped, std::memory_order_release);
    m_StatusCode.store(Audio::AudioStatusCode::Ok, std::memory_order_release);
    m_PlatformCode.store(0, std::memory_order_release);
    return {};
}

Audio::AudioStatus VoiceMixerAudioController::Restart() {
    (void)Stop();
    // Stop always releases both stream objects before it reports a platform
    // failure. Recovery is therefore allowed to attempt a clean reopen.
    m_State.store(VoiceMixerAudioState::Stopped, std::memory_order_release);
    return Start();
}

void VoiceMixerAudioController::Poll() noexcept {
    if (State() != VoiceMixerAudioState::Running || !m_Input || !m_Output) {
        return;
    }
    const auto inputState = m_Input->State();
    const auto outputState = m_Output->State();
    if (inputState == Audio::AudioStreamState::DeviceLost ||
        outputState == Audio::AudioStreamState::DeviceLost) {
        m_StatusCode.store(Audio::AudioStatusCode::DeviceLost, std::memory_order_release);
        m_State.store(VoiceMixerAudioState::DeviceLost, std::memory_order_release);
    } else if (inputState == Audio::AudioStreamState::Error ||
        outputState == Audio::AudioStreamState::Error) {
        m_StatusCode.store(Audio::AudioStatusCode::PlatformError, std::memory_order_release);
        m_State.store(VoiceMixerAudioState::Error, std::memory_order_release);
    }
}

void VoiceMixerAudioController::ApplyParameters(
    const VoiceMixerParameters& parameters) noexcept {
    m_Parameters = parameters;
    m_Voice.SetParameter(Audio::AudioParameterId::Wet, parameters.wet);
    m_Voice.SetParameter(Audio::AudioParameterId::Gain, parameters.inputGain);
    m_Voice.SetParameter(Audio::AudioParameterId::OutputGain, parameters.outputGain);
    m_Voice.SetParameter(Audio::AudioParameterId::PitchSemitones, parameters.pitchSemitones);
    m_Voice.SetParameter(Audio::AudioParameterId::FormantSemitones, parameters.formantSemitones);
    m_Voice.SetParameter(Audio::AudioParameterId::Bypass, parameters.bypass ? 1.0f : 0.0f);
    m_Voice.SetAnalyzerEnabled(parameters.analyzerEnabled);
    const Audio::NoiseSuppressorConfig noise = Audio::ClampNoiseSuppressorConfig({
        parameters.noiseEnabled,
        Audio::NoiseSuppressorMode::RNNoise,
        parameters.noiseStrength,
        parameters.noiseWet});
    if (noise.enabled != m_NoiseConfig.enabled || noise.mode != m_NoiseConfig.mode ||
        noise.strength != m_NoiseConfig.strength || noise.wet != m_NoiseConfig.wet) {
        m_NoiseConfig = noise;
        (void)m_Voice.PublishNoiseSuppression(
            Audio::BuildNoiseSuppressorSnapshot(m_NoiseConfig, ++m_NoiseRevision));
    }
    const Audio::SoftHazeConfig softHaze = Audio::ClampSoftHazeConfig(parameters.softHaze);
    if (softHaze.enabled != m_SoftHazeConfig.enabled ||
        softHaze.amount != m_SoftHazeConfig.amount || softHaze.blur != m_SoftHazeConfig.blur ||
        softHaze.damping != m_SoftHazeConfig.damping || softHaze.wet != m_SoftHazeConfig.wet) {
        m_SoftHazeConfig = softHaze;
        (void)m_Voice.PublishSoftHaze(Audio::BuildSoftHazeSnapshot(
            m_SoftHazeConfig, ++m_SoftHazeRevision));
    }
    const Audio::SpatialHazeConfig spatialHaze =
        Audio::ClampSpatialHazeConfig(parameters.spatialHaze);
    if (spatialHaze.enabled != m_SpatialHazeConfig.enabled ||
        spatialHaze.amount != m_SpatialHazeConfig.amount || spatialHaze.size != m_SpatialHazeConfig.size ||
        spatialHaze.diffusion != m_SpatialHazeConfig.diffusion ||
        spatialHaze.damping != m_SpatialHazeConfig.damping ||
        spatialHaze.width != m_SpatialHazeConfig.width || spatialHaze.wet != m_SpatialHazeConfig.wet) {
        m_SpatialHazeConfig = spatialHaze;
        (void)m_Voice.PublishSpatialHaze(Audio::BuildSpatialHazeSnapshot(
            m_SpatialHazeConfig, ++m_SpatialHazeRevision));
    }
    const Audio::SyntheticVoiceConfig syntheticVoice =
        Audio::ClampSyntheticVoiceConfig(parameters.syntheticVoice);
    if (syntheticVoice.enabled != m_SyntheticVoiceConfig.enabled ||
        syntheticVoice.amount != m_SyntheticVoiceConfig.amount ||
        syntheticVoice.stability != m_SyntheticVoiceConfig.stability ||
        syntheticVoice.electronic != m_SyntheticVoiceConfig.electronic ||
        syntheticVoice.width != m_SyntheticVoiceConfig.width ||
        syntheticVoice.wet != m_SyntheticVoiceConfig.wet) {
        m_SyntheticVoiceConfig = syntheticVoice;
        (void)m_Voice.PublishSyntheticVoice(Audio::BuildSyntheticVoiceSnapshot(
            m_SyntheticVoiceConfig, ++m_SyntheticVoiceRevision));
    }
    const Audio::ElectronicMistConfig electronicMist =
        Audio::ClampElectronicMistConfig(parameters.electronicMist);
    if (electronicMist.enabled != m_ElectronicMistConfig.enabled ||
        electronicMist.amount != m_ElectronicMistConfig.amount ||
        electronicMist.follow != m_ElectronicMistConfig.follow ||
        electronicMist.sharpness != m_ElectronicMistConfig.sharpness ||
        electronicMist.width != m_ElectronicMistConfig.width ||
        electronicMist.wet != m_ElectronicMistConfig.wet ||
        electronicMist.noiseType != m_ElectronicMistConfig.noiseType) {
        m_ElectronicMistConfig = electronicMist;
        (void)m_Voice.PublishElectronicMist(Audio::BuildElectronicMistSnapshot(
            m_ElectronicMistConfig, m_ElectronicNoiseCurve, ++m_ElectronicMistRevision));
    }
    m_Voice.SetNoiseBypassed(parameters.noiseBypassed);
    const bool monitor = parameters.monitor &&
        !m_MonitorSafetyActive.load(std::memory_order_acquire);
    m_Monitor.store(monitor, std::memory_order_release);
}

Audio::AudioStatus VoiceMixerAudioController::ApplyCurveSnapshots(
    std::shared_ptr<const Engine::Curve::CurveSnapshot> pitch,
    std::shared_ptr<const Engine::Curve::CurveSnapshot> formant,
    std::shared_ptr<const Engine::Curve::CurveSnapshot> electronicNoise) {
    if (electronicNoise) m_ElectronicNoiseCurve = std::move(electronicNoise);
    const Audio::AudioStatus curves = m_Voice.PublishCurves({std::move(pitch), std::move(formant)});
    if (!curves) return curves;
    return m_Voice.PublishElectronicMist(Audio::BuildElectronicMistSnapshot(
        m_ElectronicMistConfig, m_ElectronicNoiseCurve, ++m_ElectronicMistRevision));
}

Audio::AudioStatus VoiceMixerAudioController::ApplyParametricEQ(
    const Audio::ParametricEQModel& model) {
    m_EQModel = model;
    return m_Voice.PublishParametricEQ(Audio::BuildParametricEQSnapshot(
        m_EQModel, m_Format.sampleRate > 0 ? m_Format.sampleRate : 48'000,
        ++m_EQRevision));
}

VoiceMixerAudioSnapshot VoiceMixerAudioController::CaptureSnapshot() const noexcept {
    VoiceMixerAudioSnapshot result;
    result.state = State();
    result.lastStatus.code = m_StatusCode.load(std::memory_order_acquire);
    result.lastStatus.platformCode = m_PlatformCode.load(std::memory_order_acquire);
    result.format = m_Format;
    result.voice = m_Voice.CaptureMetrics();
    result.diagnostics = m_Diagnostics.Capture();
    result.inputPeak = m_InputPeak.load(std::memory_order_acquire);
    result.outputPeak = result.voice.outputPeak;
    result.underruns = m_Underruns.load(std::memory_order_acquire);
    result.rtAllocationsObserved = m_RtAllocations.load(std::memory_order_acquire);
    result.spectrum = m_Voice.CaptureSpectrum();
    const double deadline = result.format.sampleRate > 0
        ? 1000.0 * result.format.framesPerBuffer / result.format.sampleRate
        : 0.0;
    result.deadlinePercent = deadline > 0.0
        ? result.diagnostics.maxCallbackMilliseconds * 100.0 / deadline
        : 0.0;
    return result;
}

void VoiceMixerAudioController::CaptureCallback(
    void* userData,
    Audio::ConstAudioBufferView input,
    std::uint64_t streamFrame) noexcept {
    auto& self = *static_cast<VoiceMixerAudioController*>(userData);
    const auto begin = std::chrono::steady_clock::now();
    float peak = 0.0f;
    for (std::size_t channel = 0; channel < input.ChannelCount(); ++channel) {
        for (const float sample : input.Channel(channel)) {
            peak = std::max(peak, std::abs(sample));
        }
    }
    self.m_InputPeak.store(peak, std::memory_order_relaxed);

    Audio::AudioBufferView full = self.m_ProcessBuffer.View();
    Audio::AudioBufferView block(
        full.Data(), full.ChannelCount(), input.FrameCount(), full.ChannelStride());
    for (std::size_t channel = 0; channel < block.ChannelCount(); ++channel) {
        const std::size_t source = std::min(channel, input.ChannelCount() - 1);
        std::copy_n(input.Channel(source).data(), input.FrameCount(), block.Channel(channel).data());
    }
    Audio::AudioProcessContext context{block, block, self.m_Format.sampleRate, streamFrame};
    self.m_Voice.Process(context);
    const Audio::ConstAudioBufferView processed(
        block.Data(), block.ChannelCount(), block.FrameCount(), block.ChannelStride());
    const std::size_t pushed = self.m_Ring.Push(processed);
    if (pushed < processed.FrameCount()) {
        self.m_Diagnostics.RecordXRun(processed.FrameCount() - pushed);
    }
    const auto end = std::chrono::steady_clock::now();
    self.m_Diagnostics.RecordCallback(
        std::chrono::duration<double, std::milli>(end - begin).count());
    const auto metrics = self.m_Voice.CaptureMetrics();
    self.m_Diagnostics.RecordPitch(
        metrics.state.pitch.frequencyHz, metrics.state.pitch.confidence);
}

void VoiceMixerAudioController::RenderCallback(
    void* userData,
    Audio::AudioBufferView output,
    std::uint64_t) noexcept {
    auto& self = *static_cast<VoiceMixerAudioController*>(userData);
    if (!self.m_Monitor.load(std::memory_order_acquire)) {
        output.Clear();
        return;
    }
    if (!self.m_OutputPrimed.load(std::memory_order_acquire)) {
        if (self.m_Ring.AvailableFrames() < output.FrameCount()) {
            output.Clear();
            return;
        }
        self.m_OutputPrimed.store(true, std::memory_order_release);
    }
    const std::size_t popped = self.m_Ring.Pop(output);
    if (popped < output.FrameCount()) {
        self.m_Underruns.fetch_add(1, std::memory_order_relaxed);
        self.m_Diagnostics.RecordXRun(output.FrameCount() - popped);
    }
}

void VoiceMixerAudioController::SetFailure(Audio::AudioStatus status) noexcept {
    m_MonitorSafetyActive.store(false, std::memory_order_release);
    m_StatusCode.store(status.code, std::memory_order_release);
    m_PlatformCode.store(status.platformCode, std::memory_order_release);
    m_State.store(
        status.code == Audio::AudioStatusCode::DeviceLost
            ? VoiceMixerAudioState::DeviceLost
            : VoiceMixerAudioState::Error,
        std::memory_order_release);
}

const char* ToString(VoiceMixerAudioState state) noexcept {
    switch (state) {
    case VoiceMixerAudioState::Stopped: return "Stopped";
    case VoiceMixerAudioState::Starting: return "Starting";
    case VoiceMixerAudioState::Running: return "Running";
    case VoiceMixerAudioState::Stopping: return "Stopping";
    case VoiceMixerAudioState::DeviceLost: return "Device Lost";
    case VoiceMixerAudioState::Error: return "Error";
    }
    return "Unknown";
}

} // namespace XiE::VoiceMixer
