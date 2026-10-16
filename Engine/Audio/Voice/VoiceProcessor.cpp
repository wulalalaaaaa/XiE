#include "Audio/Voice/VoiceProcessor.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace Engine::Audio {
namespace {

void CopyBlock(AudioBufferView source, AudioBufferView destination) noexcept {
    const std::size_t channels = std::min(source.ChannelCount(), destination.ChannelCount());
    const std::size_t frames = std::min(source.FrameCount(), destination.FrameCount());
    for (std::size_t channel = 0; channel < channels; ++channel) {
        std::copy_n(source.Channel(channel).data(), frames, destination.Channel(channel).data());
    }
}

} // namespace

VoiceProcessor::VoiceProcessor()
    : m_PitchShifter(512) {}

VoiceProcessor::VoiceProcessor(VoiceProcessorConfig config)
    : m_Config(config), m_PitchDetector(config.pitchDetector),
      m_PitchShifter(config.pitchShiftWindowFrames) {}

AudioStatus VoiceProcessor::Prepare(const AudioFormat& format) {
    VoiceProcessorConfig config = m_Config;
    config.format = format;
    return Prepare(config);
}

AudioStatus VoiceProcessor::Prepare(const VoiceProcessorConfig& config) {
    if (!config.format.IsValid()) {
        return {AudioStatusCode::InvalidArgument, 0};
    }
    m_Config = config;
    m_PitchDetector.SetConfig(config.pitchDetector);
    m_PitchShifter.SetWindowFrames(config.pitchShiftWindowFrames);
    m_DryBuffer.Resize(config.format.channelCount, config.format.framesPerBuffer);

    for (IAudioProcessor* processor : {
            static_cast<IAudioProcessor*>(&m_DcBlocker),
            static_cast<IAudioProcessor*>(&m_NoiseSuppressor),
            static_cast<IAudioProcessor*>(&m_PitchShifter),
            static_cast<IAudioProcessor*>(&m_FormantShifter),
             static_cast<IAudioProcessor*>(&m_SyntheticVoice),
             static_cast<IAudioProcessor*>(&m_ParametricEQ),
             static_cast<IAudioProcessor*>(&m_ElectronicMist),
             static_cast<IAudioProcessor*>(&m_SoftHaze),
            static_cast<IAudioProcessor*>(&m_SpatialHaze),
            static_cast<IAudioProcessor*>(&m_Limiter),
            static_cast<IAudioProcessor*>(&m_Meter)}) {
        const AudioStatus result = processor->Prepare(config.format);
        if (!result) {
            return result;
        }
    }
    AudioStatus detectorStatus = m_PitchDetector.Prepare(config.format);
    if (!detectorStatus) {
        return detectorStatus;
    }
    AudioStatus analyzerStatus = m_Analyzer.Prepare(config.format, config.analyzer);
    if (!analyzerStatus) {
        return analyzerStatus;
    }
    const AudioStatus eqStatus = m_ParametricEQ.Publish(
        BuildParametricEQSnapshot(config.parametricEQ, config.format.sampleRate, 1));
    if (!eqStatus) {
        return eqStatus;
    }
    const AudioStatus softHazeStatus = m_SoftHaze.Publish(
        BuildSoftHazeSnapshot(config.softHaze, 1));
    if (!softHazeStatus) return softHazeStatus;
    const AudioStatus spatialHazeStatus = m_SpatialHaze.Publish(
        BuildSpatialHazeSnapshot(config.spatialHaze, 1));
    if (!spatialHazeStatus) return spatialHazeStatus;
    const AudioStatus syntheticVoiceStatus = m_SyntheticVoice.Publish(
        BuildSyntheticVoiceSnapshot(config.syntheticVoice, 1));
    if (!syntheticVoiceStatus) return syntheticVoiceStatus;
    const AudioStatus electronicMistStatus = m_ElectronicMist.Publish(
        BuildElectronicMistSnapshot(config.electronicMist, 1));
    if (!electronicMistStatus) return electronicMistStatus;
    const AudioStatus noiseStatus = m_NoiseSuppressor.Publish(
        BuildNoiseSuppressorSnapshot(config.noiseSuppression, 1));
    if (!noiseStatus) return noiseStatus;
    m_InputGainSmoother.Prepare(config.format.sampleRate, 10.0, 1.0f);
    m_WetSmoother.Prepare(config.format.sampleRate, 10.0, 1.0f);
    m_OutputGainSmoother.Prepare(config.format.sampleRate, 10.0, 1.0f);
    m_HoldFrames = static_cast<std::uint32_t>(
        static_cast<double>(config.format.sampleRate) *
        config.lowConfidenceHoldMilliseconds / 1000.0);

    m_Parameters.Store(AudioParameterId::Wet, 1.0f);
    m_Parameters.Store(AudioParameterId::Gain, 1.0f);
    m_Parameters.Store(AudioParameterId::PitchSemitones, 0.0f);
    m_Parameters.Store(AudioParameterId::FormantSemitones, 0.0f);
    m_Parameters.Store(AudioParameterId::OutputGain, 1.0f);
    m_Parameters.Store(AudioParameterId::Bypass, 0.0f);
    m_Prepared = true;
    Reset();
    return {};
}

void VoiceProcessor::SetParameter(AudioParameterId id, float value) noexcept {
    switch (id) {
    case AudioParameterId::Wet:
        value = std::clamp(value, 0.0f, 1.0f);
        break;
    case AudioParameterId::Gain:
    case AudioParameterId::OutputGain:
        value = std::clamp(value, 0.0f, 4.0f);
        break;
    case AudioParameterId::PitchSemitones:
        value = std::clamp(value, -12.0f, 12.0f);
        break;
    case AudioParameterId::FormantSemitones:
        value = std::clamp(value, -6.0f, 6.0f);
        break;
    case AudioParameterId::Bypass:
        value = value >= 0.5f ? 1.0f : 0.0f;
        break;
    default:
        break;
    }
    m_Parameters.Store(id, value);
}

AudioStatus VoiceProcessor::PublishCurves(const VoiceCurveSet& curves) {
    if (curves.pitch) {
        AudioStatus result = m_PitchCurve.Publish(curves.pitch);
        if (!result) {
            return result;
        }
    }
    if (curves.formant) {
        AudioStatus result = m_FormantCurve.Publish(curves.formant);
        if (!result) {
            return result;
        }
    }
    if (curves.electronicNoise) {
        // The VoiceProcessor owns the immutable curve conversion.  The
        // ElectronicMist snapshot also carries the parameter configuration;
        // publishing a curve alone therefore preserves the current config.
        AudioStatus result = m_ElectronicMist.Publish(BuildElectronicMistSnapshot(
            m_Config.electronicMist, curves.electronicNoise,
            m_ElectronicMist.CaptureMetrics().snapshotRevision + 1));
        if (!result) return result;
    }
    return {};
}

AudioStatus VoiceProcessor::PublishParametricEQ(
    std::shared_ptr<const ParametricEQSnapshot> snapshot) {
    return m_ParametricEQ.Publish(std::move(snapshot));
}

AudioStatus VoiceProcessor::PublishNoiseSuppression(
    std::shared_ptr<const NoiseSuppressorSnapshot> snapshot) {
    return m_NoiseSuppressor.Publish(std::move(snapshot));
}

AudioStatus VoiceProcessor::PublishSoftHaze(
    std::shared_ptr<const SoftHazeSnapshot> snapshot) {
    return m_SoftHaze.Publish(std::move(snapshot));
}

AudioStatus VoiceProcessor::PublishSpatialHaze(
    std::shared_ptr<const SpatialHazeSnapshot> snapshot) {
    return m_SpatialHaze.Publish(std::move(snapshot));
}

AudioStatus VoiceProcessor::PublishSyntheticVoice(
    std::shared_ptr<const SyntheticVoiceSnapshot> snapshot) {
    return m_SyntheticVoice.Publish(std::move(snapshot));
}

AudioStatus VoiceProcessor::PublishElectronicMist(
    std::shared_ptr<const ElectronicMistSnapshot> snapshot) {
    if (snapshot) m_Config.electronicMist = snapshot->config;
    return m_ElectronicMist.Publish(std::move(snapshot));
}

void VoiceProcessor::Process(AudioProcessContext& context) noexcept {
    if (!m_Prepared) {
        context.output.Clear();
        return;
    }
    AudioBufferView dryFull = m_DryBuffer.View();
    AudioBufferView dry(
        dryFull.Data(),
        dryFull.ChannelCount(),
        context.output.FrameCount(),
        dryFull.ChannelStride());
    CopyBlock(context.output, dry);
    const ConstAudioBufferView inputAnalysis(
        context.output.Data(), context.output.ChannelCount(),
        context.output.FrameCount(), context.output.ChannelStride());
    m_Analyzer.PushInput(inputAnalysis);

    const AudioParameterSnapshot parameters = m_Parameters.Capture();
    m_LastParameterGeneration.store(parameters.generation, std::memory_order_relaxed);
    const bool globalBypass = Bypassed() ||
        parameters.Get(AudioParameterId::Bypass) >= 0.5f;
    m_GlobalBypass.store(globalBypass, std::memory_order_relaxed);

    if (!m_DcBlocker.Bypassed()) {
        m_DcBlocker.Process(context);
    }
    m_NoiseSuppressor.Process(context);

    m_InputGainSmoother.SetTarget(parameters.Get(AudioParameterId::Gain));
    for (std::size_t frame = 0; frame < context.output.FrameCount(); ++frame) {
        const float gain = m_InputGainSmoother.Next();
        for (std::size_t channel = 0; channel < context.output.ChannelCount(); ++channel) {
            context.output.Sample(channel, frame) *= gain;
        }
    }

    PitchEstimate estimate = m_PitchDetector.Latest();
    if (!m_PitchDetectorBypassed.load(std::memory_order_acquire)) {
        const ConstAudioBufferView analysis(
            context.output.Data(),
            context.output.ChannelCount(),
            context.output.FrameCount(),
            context.output.ChannelStride());
        estimate = m_PitchDetector.Process(analysis);
    }
    m_DetectedFrequency.store(estimate.frequencyHz, std::memory_order_relaxed);
    m_PitchConfidence.store(estimate.confidence, std::memory_order_relaxed);
    m_PitchVoiced.store(estimate.voiced, std::memory_order_relaxed);
    const PitchStabilizerResult stabilized = m_SyntheticVoice.UpdatePitch(
        estimate, static_cast<std::uint32_t>(context.output.FrameCount()));

    const float directPitch = parameters.Get(AudioParameterId::PitchSemitones);
    const float directFormant = parameters.Get(AudioParameterId::FormantSemitones);
    if (estimate.voiced && estimate.confidence >= m_Config.minimumPitchConfidence) {
        m_HeldPitchSemitones = directPitch +
            m_PitchCurve.Evaluate(estimate.frequencyHz, m_Config.fallbackPitchSemitones) +
            stabilized.correctionCents / 100.0f;
        m_HeldFormantSemitones = directFormant +
            m_FormantCurve.Evaluate(estimate.frequencyHz, m_Config.fallbackFormantSemitones);
        m_LowConfidenceFrames = 0;
    } else {
        m_LowConfidenceFrames = std::min<std::uint32_t>(
            m_HoldFrames + 1,
            m_LowConfidenceFrames + static_cast<std::uint32_t>(context.output.FrameCount()));
        if (m_LowConfidenceFrames > m_HoldFrames) {
            m_HeldPitchSemitones = directPitch + m_Config.fallbackPitchSemitones +
                stabilized.correctionCents / 100.0f;
            m_HeldFormantSemitones = directFormant + m_Config.fallbackFormantSemitones;
        }
    }
    m_HeldPitchSemitones = std::clamp(m_HeldPitchSemitones, -12.0f, 12.0f);
    m_HeldFormantSemitones = std::clamp(m_HeldFormantSemitones, -6.0f, 6.0f);
    m_TargetPitch.store(m_HeldPitchSemitones, std::memory_order_relaxed);
    m_TargetFormant.store(m_HeldFormantSemitones, std::memory_order_relaxed);

    m_PitchShifter.SetPitchSemitones(m_HeldPitchSemitones);
    if (!m_PitchShifter.Bypassed()) {
        m_PitchShifter.Process(context);
    }
    m_FormantShifter.SetFormantSemitones(m_HeldFormantSemitones);
    if (!m_FormantShifter.Bypassed()) {
        m_FormantShifter.Process(context);
    }
    if (!m_SyntheticVoice.Bypassed()) {
        m_SyntheticVoice.Process(context);
    }
    if (!m_ParametricEQ.Bypassed()) {
        m_ParametricEQ.Process(context);
    }
    m_ElectronicMist.SetVoiceEstimate(estimate);
    if (!m_ElectronicMist.Bypassed()) {
        m_ElectronicMist.Process(context);
    }
    if (!m_SoftHaze.Bypassed()) {
        m_SoftHaze.Process(context);
    }
    if (!m_SpatialHaze.Bypassed()) {
        m_SpatialHaze.Process(context);
    }
    if (!m_Limiter.Bypassed()) {
        m_Limiter.Process(context);
    }

    m_WetSmoother.SetTarget(globalBypass ? 0.0f : parameters.Get(AudioParameterId::Wet));
    m_OutputGainSmoother.SetTarget(parameters.Get(AudioParameterId::OutputGain));
    for (std::size_t frame = 0; frame < context.output.FrameCount(); ++frame) {
        const float wet = m_WetSmoother.Next();
        const float outputGain = m_OutputGainSmoother.Next();
        for (std::size_t channel = 0; channel < context.output.ChannelCount(); ++channel) {
            context.output.Sample(channel, frame) =
                std::lerp(dry.Sample(channel, frame), context.output.Sample(channel, frame), wet) *
                outputGain;
        }
        m_CurrentWet.store(wet, std::memory_order_relaxed);
    }
    const ConstAudioBufferView outputAnalysis(
        context.output.Data(), context.output.ChannelCount(),
        context.output.FrameCount(), context.output.ChannelStride());
    m_Analyzer.PushOutput(outputAnalysis);
    m_Meter.Process(context);
    m_ProcessedFrames.fetch_add(context.output.FrameCount(), std::memory_order_relaxed);
}

void VoiceProcessor::Reset() noexcept {
    m_DcBlocker.Reset();
    m_NoiseSuppressor.Reset();
    m_PitchDetector.Reset();
    m_PitchShifter.Reset();
    m_FormantShifter.Reset();
    m_SyntheticVoice.Reset();
    m_ParametricEQ.Reset();
    m_ElectronicMist.Reset();
    m_SoftHaze.Reset();
    m_SpatialHaze.Reset();
    m_Limiter.Reset();
    m_Meter.Reset();
    m_InputGainSmoother.Reset(1.0f);
    m_WetSmoother.Reset(1.0f);
    m_OutputGainSmoother.Reset(1.0f);
    m_HeldPitchSemitones = 0.0f;
    m_HeldFormantSemitones = 0.0f;
    m_LowConfidenceFrames = 0;
    m_ProcessedFrames.store(0, std::memory_order_release);
}

VoiceProcessorMetrics VoiceProcessor::CaptureMetrics() const noexcept {
    VoiceProcessorMetrics result;
    result.state.pitch.frequencyHz = m_DetectedFrequency.load(std::memory_order_acquire);
    result.state.pitch.confidence = m_PitchConfidence.load(std::memory_order_acquire);
    result.state.pitch.voiced = m_PitchVoiced.load(std::memory_order_acquire);
    result.state.targetPitchSemitones = m_TargetPitch.load(std::memory_order_acquire);
    result.state.targetFormantSemitones = m_TargetFormant.load(std::memory_order_acquire);
    result.state.wet = m_CurrentWet.load(std::memory_order_acquire);
    result.state.bypassed = m_GlobalBypass.load(std::memory_order_acquire);
    result.state.parameterGeneration = m_LastParameterGeneration.load(std::memory_order_acquire);
    result.state.eqEnabled = m_ParametricEQ.CaptureMetrics().enabled;
    result.state.noiseEnabled = m_NoiseSuppressor.CaptureMetrics().enabled;
    result.state.softHazeEnabled = m_SoftHaze.CaptureMetrics().enabled;
    result.state.spatialHazeEnabled = m_SpatialHaze.CaptureMetrics().enabled;
    result.state.syntheticVoiceEnabled = m_SyntheticVoice.CaptureMetrics().enabled;
    result.state.electronicMistEnabled = m_ElectronicMist.CaptureMetrics().enabled;
    result.outputPeak = m_Meter.Peak();
    result.outputRms = m_Meter.Rms();
    result.processedFrames = m_ProcessedFrames.load(std::memory_order_acquire);
    result.algorithmLatencyFrames = LatencyFrames();
    result.eq = m_ParametricEQ.CaptureMetrics();
    result.noise = m_NoiseSuppressor.CaptureMetrics();
    result.softHaze = m_SoftHaze.CaptureMetrics();
    result.spatialHaze = m_SpatialHaze.CaptureMetrics();
    result.syntheticVoice = m_SyntheticVoice.CaptureMetrics();
    result.electronicMist = m_ElectronicMist.CaptureMetrics();
    result.analyzer = m_Analyzer.CaptureMetrics();
    return result;
}

std::string DumpVoiceState(const VoiceProcessorMetrics& metrics) {
    std::ostringstream output;
    output << "DetectedF0=" << metrics.state.pitch.frequencyHz << '\n'
           << "PitchConfidence=" << metrics.state.pitch.confidence << '\n'
           << "Voiced=" << (metrics.state.pitch.voiced ? "true" : "false") << '\n'
           << "TargetPitchSemitones=" << metrics.state.targetPitchSemitones << '\n'
           << "TargetFormantSemitones=" << metrics.state.targetFormantSemitones << '\n'
           << "Wet=" << metrics.state.wet << '\n'
           << "Bypassed=" << (metrics.state.bypassed ? "true" : "false") << '\n'
           << "EQEnabled=" << (metrics.eq.enabled ? "true" : "false") << '\n'
           << "EQBandCount=" << metrics.eq.bandCount << '\n'
           << "EQSnapshotRevision=" << metrics.eq.snapshotRevision << '\n'
           << "EQProcessMilliseconds=" << metrics.eq.processMilliseconds << '\n'
           << "NoiseMode=" << ToString(metrics.noise.mode) << '\n'
           << "NoiseState=" << ToString(metrics.noise.state) << '\n'
           << "NoiseEnabled=" << (metrics.noise.enabled ? "true" : "false") << '\n'
           << "NoiseBypassed=" << (metrics.noise.bypassed ? "true" : "false") << '\n'
           << "NoiseStrength=" << metrics.noise.strength << '\n'
           << "NoiseWet=" << metrics.noise.wet << '\n'
           << "NoiseVoiceProbability=" << metrics.noise.voiceProbability << '\n'
           << "NoiseAverageAttenuationDb=" << metrics.noise.averageAttenuationDb << '\n'
           << "NoisePeakAttenuationDb=" << metrics.noise.peakAttenuationDb << '\n'
           << "NoiseFrameSize=" << metrics.noise.frameSize << '\n'
           << "NoiseBufferedInputFrames=" << metrics.noise.bufferedInputFrames << '\n'
           << "NoiseBufferedOutputFrames=" << metrics.noise.bufferedOutputFrames << '\n'
           << "NoiseLatencyMilliseconds=" << metrics.noise.algorithmicLatencyMilliseconds << '\n'
           << "NoiseProcessAverageMilliseconds=" << metrics.noise.averageProcessMilliseconds << '\n'
           << "NoiseProcessP95Milliseconds=" << metrics.noise.p95ProcessMilliseconds << '\n'
           << "NoiseProcessP99Milliseconds=" << metrics.noise.p99ProcessMilliseconds << '\n'
           << "NoiseProcessMaximumMilliseconds=" << metrics.noise.maximumProcessMilliseconds << '\n'
           << "NoiseDroppedFrames=" << metrics.noise.droppedFrames << '\n'
           << "NoiseRTAllocations=" << metrics.noise.rtAllocations << '\n'
           << "SoftHazeEnabled=" << (metrics.softHaze.enabled ? "true" : "false") << '\n'
           << "SoftHazeAmount=" << metrics.softHaze.amount << '\n'
           << "SoftHazeBlur=" << metrics.softHaze.blur << '\n'
           << "SoftHazeDamping=" << metrics.softHaze.damping << '\n'
           << "SoftHazeWet=" << metrics.softHaze.wet << '\n'
           << "SoftHazeSnapshotRevision=" << metrics.softHaze.snapshotRevision << '\n'
           << "SoftHazeEffectiveDelayMilliseconds=" << metrics.softHaze.effectiveDelayMilliseconds << '\n'
           << "SoftHazeProcessMilliseconds=" << metrics.softHaze.processMilliseconds << '\n'
           << "SpatialHazeEnabled=" << (metrics.spatialHaze.enabled ? "true" : "false") << '\n'
           << "SpatialHazeAmount=" << metrics.spatialHaze.amount << '\n'
           << "SpatialHazeSize=" << metrics.spatialHaze.size << '\n'
           << "SpatialHazeDiffusion=" << metrics.spatialHaze.diffusion << '\n'
           << "SpatialHazeDamping=" << metrics.spatialHaze.damping << '\n'
           << "SpatialHazeWidth=" << metrics.spatialHaze.width << '\n'
           << "SpatialHazeWet=" << metrics.spatialHaze.wet << '\n'
           << "SpatialHazeSnapshotRevision=" << metrics.spatialHaze.snapshotRevision << '\n'
           << "SpatialHazeEffectiveTailMilliseconds=" << metrics.spatialHaze.effectiveTailMilliseconds << '\n'
           << "SpatialHazeProcessMilliseconds=" << metrics.spatialHaze.processMilliseconds << '\n'
           << "SyntheticEnabled=" << (metrics.syntheticVoice.enabled ? "true" : "false") << '\n'
           << "SyntheticAmount=" << metrics.syntheticVoice.amount << '\n'
           << "SyntheticStability=" << metrics.syntheticVoice.stability << '\n'
           << "SyntheticElectronic=" << metrics.syntheticVoice.electronic << '\n'
           << "SyntheticWidth=" << metrics.syntheticVoice.width << '\n'
           << "SyntheticWet=" << metrics.syntheticVoice.wet << '\n'
           << "SyntheticRawF0=" << metrics.syntheticVoice.rawFrequencyHz << '\n'
           << "SyntheticStabilizedF0=" << metrics.syntheticVoice.stabilizedFrequencyHz << '\n'
           << "SyntheticCorrectionCents=" << metrics.syntheticVoice.correctionCents << '\n'
           << "SyntheticConfidence=" << metrics.syntheticVoice.confidence << '\n'
           << "SyntheticVoiced=" << (metrics.syntheticVoice.voiced ? "true" : "false") << '\n'
           << "SyntheticCorrectionHeld=" << (metrics.syntheticVoice.correctionHeld ? "true" : "false") << '\n'
           << "SyntheticControlSmoothingMilliseconds=" << metrics.syntheticVoice.controlSmoothingMilliseconds << '\n'
           << "MicroPitchActive=" << (metrics.syntheticVoice.micro.enabled ? "true" : "false") << '\n'
           << "MicroPitchDetuneCents=" << metrics.syntheticVoice.micro.detuneCents << '\n'
           << "MicroPitchDelaySamples=" << metrics.syntheticVoice.micro.delaySamples << '\n'
            << "MicroPitchLatencyFrames=" << metrics.syntheticVoice.micro.algorithmicLatencyFrames << '\n'
            << "SyntheticProcessMilliseconds=" << metrics.syntheticVoice.processMilliseconds << '\n'
            << "MistEnabled=" << (metrics.electronicMist.enabled ? "true" : "false") << '\n'
            << "MistAmount=" << metrics.electronicMist.amount << '\n'
            << "MistFollow=" << metrics.electronicMist.follow << '\n'
            << "MistSharpness=" << metrics.electronicMist.sharpness << '\n'
            << "MistBandCount=" << metrics.electronicMist.bandCount << '\n'
            << "MistActiveBands=" << metrics.electronicMist.activeBandCount << '\n'
            << "MistVoiceEnvelope=" << metrics.electronicMist.broadbandVoiceEnvelope << '\n'
            << "MistGateActivity=" << metrics.electronicMist.voiceGateActivity << '\n'
            << "MistRms=" << metrics.electronicMist.mistRms << '\n'
            << "MistVoiceRms=" << metrics.electronicMist.voiceRms << '\n'
            << "MistVoiceRatioDb=" << metrics.electronicMist.mistToVoiceDb << '\n'
            << "MistSnapshotRevision=" << metrics.electronicMist.snapshotRevision << '\n'
            << "MistProcessMilliseconds=" << metrics.electronicMist.processMilliseconds << '\n'
            << "AnalyzerFFTSize=" << metrics.analyzer.fftSize << '\n'
           << "SpectrumRevision=" << metrics.analyzer.revision << '\n'
           << "OutputPeak=" << metrics.outputPeak << '\n'
           << "OutputRMS=" << metrics.outputRms << '\n'
           << "ProcessedFrames=" << metrics.processedFrames << '\n'
           << "AlgorithmLatencyFrames=" << metrics.algorithmLatencyFrames << '\n';
    return output.str();
}

} // namespace Engine::Audio
