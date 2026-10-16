#include "Audio/Core/AudioBuffer.h"
#include "Audio/Core/AudioParameter.h"
#include "Audio/Diagnostics/AudioDiagnostics.h"
#include "Audio/DSP/BasicProcessors.h"
#include "Audio/DSP/Pitch.h"
#include "Audio/Graph/AudioGraph.h"
#include "Audio/Voice/VoiceProcessor.h"
#include "Curve/Curve.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <numbers>

namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        ++gFailures;
        std::cerr << "[FAIL] " << message << '\n';
    } else {
        std::cout << "[PASS] " << message << '\n';
    }
}

bool Near(float actual, float expected, float tolerance = 1.0e-3f) {
    return std::fabs(actual - expected) <= tolerance;
}

void TestCurve() {
    using namespace Engine::Curve;
    CurveModel curve;
    Expect(curve.AddPoint({2, 1.0f, 1.0f, CurveInterpolation::Linear}) == CurveStatus::Ok,
        "curve accepts first point");
    Expect(curve.AddPoint({1, 0.0f, 0.0f, CurveInterpolation::Linear}) == CurveStatus::Ok,
        "curve accepts and sorts second point");
    Expect(curve.Points().front().id == 1, "curve points are sorted by x");
    Expect(Near(CurveSampler::Sample(curve.Points(), 0.25f), 0.25f), "linear interpolation");
    Expect(Near(CurveSampler::Sample(curve.Points(), -1.0f), 0.0f), "curve clamps low boundary");
    Expect(Near(CurveSampler::Sample(curve.Points(), 2.0f), 1.0f), "curve clamps high boundary");
    Expect(curve.AddPoint({3, 1.0f, 2.0f}) == CurveStatus::DuplicateX,
        "curve rejects duplicate x");
    Expect(curve.AddPoint({3, std::numeric_limits<float>::quiet_NaN(), 2.0f}) ==
        CurveStatus::NonFiniteValue, "curve rejects non-finite input");
    Expect(curve.MovePoint(2, 2.0f, 2.0f) == CurveStatus::Ok, "curve moves point");
    Expect(curve.RemovePoint(999) == CurveStatus::PointNotFound, "curve reports missing removal");

    CurveModel smooth;
    smooth.AddPoint({1, 0.0f, 0.0f, CurveInterpolation::Smooth});
    smooth.AddPoint({2, 1.0f, 0.5f, CurveInterpolation::Smooth});
    smooth.AddPoint({3, 2.0f, 1.0f, CurveInterpolation::Smooth});
    float previous = 0.0f;
    bool monotonic = true;
    for (int i = 0; i <= 200; ++i) {
        const float sample = CurveSampler::Sample(smooth.Points(), static_cast<float>(i) / 100.0f);
        monotonic = monotonic && sample + 1.0e-6f >= previous && sample >= 0.0f && sample <= 1.0f;
        previous = sample;
    }
    Expect(monotonic, "smooth interpolation is monotonic and avoids overshoot");

    CurveLUT lut;
    Expect(lut.Build(smooth.Points(), 1024, 0.0f, 2.0f) == CurveStatus::Ok, "LUT builds");
    Expect(Near(lut.Sample(1.0f), 0.5f, 0.002f), "LUT interpolates");
    Expect(lut.Build(smooth.Points(), 1, 0.0f, 2.0f) == CurveStatus::InvalidLutSize,
        "LUT rejects invalid size");
    CurveStatus status{};
    const auto snapshot = BuildCurveSnapshot(smooth, 2048, 0.0f, 2.0f, &status);
    Expect(snapshot && status == CurveStatus::Ok && Near(snapshot->Sample(1.5f), 0.75f, 0.002f),
        "immutable curve snapshot samples");
}

void TestAudioBuffer() {
    using namespace Engine::Audio;
    AudioBuffer mono(1, 16);
    Expect(mono.ChannelCount() == 1 && mono.FrameCount() == 16, "mono buffer dimensions");
    mono.View().Sample(0, 3) = 0.5f;
    Expect(Near(mono.View().Sample(0, 3), 0.5f), "buffer view accesses sample");
    mono.Clear();
    Expect(Near(mono.View().Sample(0, 3), 0.0f), "buffer clear");

    AudioBuffer stereo(2, 32);
    for (std::size_t channel = 0; channel < 2; ++channel) {
        for (std::size_t frame = 0; frame < 32; ++frame) {
            stereo.View().Sample(channel, frame) = static_cast<float>(channel * 100 + frame);
        }
    }
    AudioBuffer copy(2, 32);
    Expect(copy.CopyFrom(stereo.View()), "buffer copy accepts equal dimensions");
    Expect(Near(copy.View().Sample(1, 7), 107.0f), "stereo copy preserves channels");
    AudioBuffer wrong(1, 32);
    Expect(!wrong.CopyFrom(stereo.View()), "buffer copy rejects mismatched dimensions");

    AudioBufferPool pool;
    pool.Prepare(2, 2, 8);
    Expect(!pool.Acquire().Empty() && !pool.Acquire().Empty(), "buffer pool returns prepared buffers");
    Expect(pool.Acquire().Empty(), "buffer pool bounds acquisition");
    pool.Reset();
    Expect(!pool.Acquire().Empty(), "buffer pool reset");
}

void TestParameters() {
    using namespace Engine::Audio;
    AudioParameter parameter(1.0f);
    parameter.Store(2.0f);
    Expect(Near(parameter.Load(), 2.0f), "atomic parameter update");

    AudioParameterStore store;
    store.Store(AudioParameterId::Wet, 0.75f);
    store.Store(AudioParameterId::PitchSemitones, 5.0f);
    const auto first = store.Capture();
    store.Store(AudioParameterId::Wet, 0.25f);
    const auto second = store.Capture();
    Expect(Near(first.Get(AudioParameterId::Wet), 0.75f) &&
        Near(second.Get(AudioParameterId::Wet), 0.25f) &&
        second.generation > first.generation, "parameter snapshots switch atomically");

    ParameterSmoother smoother;
    smoother.Prepare(1000.0, 10.0, 0.0f);
    smoother.SetTarget(1.0f);
    float previous = 0.0f;
    bool continuous = true;
    for (int i = 0; i < 10; ++i) {
        const float value = smoother.Next();
        continuous = continuous && value >= previous && value - previous <= 0.101f;
        previous = value;
    }
    Expect(continuous && Near(smoother.Current(), 1.0f), "parameter smoothing reaches target continuously");
}

Engine::Audio::AudioBuffer MakeConstant(std::size_t channels, std::size_t frames, float value) {
    Engine::Audio::AudioBuffer buffer(channels, frames);
    for (std::size_t channel = 0; channel < channels; ++channel) {
        std::fill(buffer.View().Channel(channel).begin(), buffer.View().Channel(channel).end(), value);
    }
    return buffer;
}

void TestGraphAndDsp() {
    using namespace Engine::Audio;
    const AudioFormat format{48'000, 2, AudioSampleFormat::Float32, 128};
    AudioBuffer input = MakeConstant(2, 128, 0.5f);
    AudioBuffer output(2, 128);
    AudioProcessContext context{input.View(), output.View(), format.sampleRate, 0};

    AudioGraph graph;
    Expect(static_cast<bool>(graph.Prepare(format)), "graph prepare");
    AudioGraphBuilder emptyBuilder;
    AudioStatus status;
    auto empty = emptyBuilder.Build(format, &status);
    Expect(empty && static_cast<bool>(graph.Publish(std::move(empty))), "empty graph publishes");
    graph.Process(context);
    Expect(Near(output.View().Sample(1, 40), 0.5f), "empty graph passes audio");

    AudioGraphBuilder singleBuilder;
    auto gain = std::make_unique<GainProcessor>(2.0f);
    GainProcessor* gainControl = gain.get();
    singleBuilder.Add(std::move(gain));
    auto single = singleBuilder.Build(format, &status);
    Expect(single && single->ProcessorCount() == 1, "single processor graph builds");
    Expect(static_cast<bool>(graph.Publish(std::move(single))), "snapshot replace publishes");
    graph.Process(context);
    graph.Process(context);
    Expect(Near(output.View().Sample(0, 127), 1.0f, 0.01f), "gain processor applies smoothed gain");
    gainControl->SetBypassed(true);
    graph.Process(context);
    Expect(Near(output.View().Sample(0, 127), 0.5f), "processor bypass passes dry audio");
    gainControl->SetBypassed(false);

    AudioGraphBuilder multiBuilder;
    multiBuilder.Add(std::make_unique<DCBlocker>());
    multiBuilder.Add(std::make_unique<Limiter>(0.8f));
    multiBuilder.Add(std::make_unique<MeterProcessor>());
    auto multi = multiBuilder.Build(format, &status);
    Expect(multi && multi->ProcessorCount() == 3, "multi processor graph builds");
    multi->Reset();

    DCBlocker dc;
    Expect(static_cast<bool>(dc.Prepare(format)), "DC blocker prepare");
    AudioBuffer dcInput = MakeConstant(2, 128, 0.25f);
    AudioBuffer dcOutput(2, 128);
    AudioProcessContext dcContext{dcInput.View(), dcOutput.View(), format.sampleRate, 0};
    for (int block = 0; block < 100; ++block) {
        std::copy(dcInput.View().Channel(0).begin(), dcInput.View().Channel(0).end(), dcOutput.View().Channel(0).begin());
        std::copy(dcInput.View().Channel(1).begin(), dcInput.View().Channel(1).end(), dcOutput.View().Channel(1).begin());
        dc.Process(dcContext);
    }
    Expect(std::fabs(dcOutput.View().Sample(0, 127)) < 0.001f, "DC blocker rejects steady DC");

    Limiter limiter(0.8f);
    limiter.Prepare(format);
    AudioBuffer loud = MakeConstant(2, 128, 2.0f);
    AudioProcessContext limiterContext{loud.View(), loud.View(), format.sampleRate, 0};
    limiter.Process(limiterContext);
    float maximum = 0.0f;
    for (float sample : loud.View().Channel(0)) {
        maximum = std::max(maximum, std::fabs(sample));
    }
    Expect(maximum <= 0.8001f, "limiter enforces ceiling");

    MeterProcessor meter;
    meter.Prepare(format);
    AudioBuffer meterSignal = MakeConstant(2, 128, 0.25f);
    AudioProcessContext meterContext{meterSignal.View(), meterSignal.View(), format.sampleRate, 0};
    meter.Process(meterContext);
    Expect(Near(meter.Peak(), 0.25f) && Near(meter.Rms(), 0.25f), "peak and RMS meter");

    ChannelMixer mixer;
    mixer.SetMatrix(2, {0.5f, 0.5f, 0.5f, 0.5f});
    mixer.Prepare(format);
    AudioBuffer stereo(2, 128);
    std::fill(stereo.View().Channel(0).begin(), stereo.View().Channel(0).end(), 1.0f);
    std::fill(stereo.View().Channel(1).begin(), stereo.View().Channel(1).end(), -1.0f);
    AudioProcessContext mixerContext{stereo.View(), stereo.View(), format.sampleRate, 0};
    mixer.Process(mixerContext);
    Expect(Near(stereo.View().Sample(0, 3), 0.0f) && Near(stereo.View().Sample(1, 3), 0.0f),
        "channel mixer matrix");

    AudioDiagnostics diagnostics;
    diagnostics.RecordCallback(0.25);
    diagnostics.RecordCallback(0.5);
    diagnostics.RecordXRun(64);
    diagnostics.RecordPitch(220.0f, 0.9f);
    diagnostics.SetLatency({2.0, 1.0, 3.0});
    const AudioMetricsSnapshot metrics = diagnostics.Capture();
    Expect(metrics.callbackCount == 2 && Near(static_cast<float>(metrics.maxCallbackMilliseconds), 0.5f),
        "diagnostics capture callback metrics");
    Expect(metrics.xRuns == 1 && metrics.droppedFrames == 64 &&
        Near(static_cast<float>(metrics.latency.TotalEstimatedMilliseconds()), 6.0f),
        "diagnostics capture xruns and latency");
}

Engine::Audio::PitchEstimate DetectSine(float frequencyHz, float amplitude = 0.5f) {
    using namespace Engine::Audio;
    const AudioFormat format{48'000, 1, AudioSampleFormat::Float32, 128};
    PitchDetector detector;
    detector.Prepare(format);
    AudioBuffer block(1, format.framesPerBuffer);
    std::uint64_t absoluteFrame = 0;
    for (int iteration = 0; iteration < 40; ++iteration) {
        for (std::size_t frame = 0; frame < block.FrameCount(); ++frame) {
            block.View().Sample(0, frame) = amplitude * std::sin(
                2.0 * std::numbers::pi * static_cast<double>(frequencyHz) *
                static_cast<double>(absoluteFrame++) / format.sampleRate);
        }
        const AudioBuffer& constant = block;
        detector.Process(constant.View());
    }
    return detector.Latest();
}

void TestPitch() {
    using namespace Engine::Audio;
    for (const float frequency : {100.0f, 220.0f, 440.0f}) {
        const PitchEstimate estimate = DetectSine(frequency);
        Expect(estimate.voiced && std::fabs(estimate.frequencyHz - frequency) < 1.0f &&
            estimate.confidence > 0.8f, "pitch detector tracks synthetic sine");
    }

    const PitchEstimate low = DetectSine(220.0f, 0.0001f);
    Expect(!low.voiced && low.frequencyHz == 0.0f, "pitch detector rejects low-amplitude signal");

    const AudioFormat format{48'000, 1, AudioSampleFormat::Float32, 128};
    PitchDetector detector;
    detector.Prepare(format);
    AudioBuffer silence(1, 128);
    const AudioBuffer& constantSilence = silence;
    for (int block = 0; block < 24; ++block) {
        detector.Process(constantSilence.View());
    }
    Expect(!detector.Latest().voiced, "pitch detector marks silence unvoiced");

    AudioBuffer noise(1, 128);
    std::uint32_t random = 0x12345678u;
    for (int block = 0; block < 40; ++block) {
        for (float& sample : noise.View().Channel(0)) {
            random = random * 1664525u + 1013904223u;
            sample = (static_cast<float>((random >> 8) & 0xffffu) / 32767.5f - 1.0f) * 0.2f;
        }
        const AudioBuffer& constantNoise = noise;
        detector.Process(constantNoise.View());
    }
    Expect(!detector.Latest().voiced || detector.Latest().confidence < 0.65f,
        "pitch detector does not report high-confidence white noise");

    Engine::Curve::CurveModel model;
    model.AddPoint({1, 80.0f, 6.0f, Engine::Curve::CurveInterpolation::Linear});
    model.AddPoint({2, 260.0f, 1.0f, Engine::Curve::CurveInterpolation::Linear});
    auto snapshot = Engine::Curve::BuildCurveSnapshot(model, 1024, 60.0f, 400.0f);
    PitchCurve pitchCurve;
    Expect(static_cast<bool>(pitchCurve.Publish(snapshot)), "pitch curve publishes immutable LUT");
    Expect(Near(pitchCurve.Evaluate(80.0f), 6.0f, 0.01f) &&
        Near(pitchCurve.Evaluate(260.0f), 1.0f, 0.01f), "pitch curve maps F0 to semitones");
    Expect(Near(PitchCurve::HertzToMidi(440.0f), 69.0f) &&
        Near(PitchCurve::MidiToHertz(69.0f), 440.0f), "pitch logarithmic semantics");

    PitchShiftProcessor shifter(256);
    Expect(static_cast<bool>(shifter.Prepare(format)), "pitch shifter prepare");
    shifter.SetPitchSemitones(7.0f);
    AudioBuffer signal(1, 128);
    std::uint64_t streamFrame = 0;
    double outputEnergy = 0.0;
    bool finiteOutput = true;
    for (int block = 0; block < 40; ++block) {
        for (std::size_t frame = 0; frame < signal.FrameCount(); ++frame) {
            signal.View().Sample(0, frame) = 0.4f * std::sin(
                2.0 * std::numbers::pi * 220.0 *
                static_cast<double>(streamFrame++) / format.sampleRate);
        }
        AudioProcessContext context{signal.View(), signal.View(), format.sampleRate, streamFrame - 128};
        shifter.Process(context);
        if (block > 5) {
            for (float sample : signal.View().Channel(0)) {
                outputEnergy += static_cast<double>(sample) * sample;
                finiteOutput = finiteOutput && std::isfinite(sample);
            }
        }
    }
    Expect(finiteOutput && outputEnergy > 1.0 && shifter.LatencyFrames() == 128,
        "pitch shifter produces time-preserving continuous output with declared latency");
}

std::shared_ptr<const Engine::Curve::CurveSnapshot> ConstantVoiceCurve(float value) {
    Engine::Curve::CurveModel model;
    model.AddPoint({1, 60.0f, value, Engine::Curve::CurveInterpolation::Linear});
    model.AddPoint({2, 500.0f, value, Engine::Curve::CurveInterpolation::Linear});
    return Engine::Curve::BuildCurveSnapshot(model, 1024, 60.0f, 500.0f);
}

void TestFormantAndVoice() {
    using namespace Engine::Audio;
    const AudioFormat format{48'000, 1, AudioSampleFormat::Float32, 128};

    FormantShiftProcessor formant;
    Expect(static_cast<bool>(formant.Prepare(format)) && formant.Experimental(),
        "formant processor is prepared and explicitly experimental");
    formant.SetFormantSemitones(4.0f);
    AudioBuffer impulse(1, 128);
    impulse.View().Sample(0, 0) = 1.0f;
    AudioProcessContext formantContext{impulse.View(), impulse.View(), format.sampleRate, 0};
    bool finite = true;
    float changedEnergy = 0.0f;
    for (int block = 0; block < 20; ++block) {
        if (block != 0) {
            impulse.Clear();
        }
        formant.Process(formantContext);
        for (float sample : impulse.View().Channel(0)) {
            finite = finite && std::isfinite(sample);
            changedEnergy += std::fabs(sample);
        }
    }
    Expect(finite && changedEnergy > 0.1f, "experimental formant warp is stable on impulse");

    VoiceProcessorConfig config;
    config.format = format;
    config.pitchShiftWindowFrames = 256;
    VoiceProcessor voice;
    Expect(static_cast<bool>(voice.Prepare(config)), "voice processor prepare");
    Expect(static_cast<bool>(voice.PublishCurves({
        ConstantVoiceCurve(5.0f),
        ConstantVoiceCurve(-1.5f)})), "voice curve set publishes");
    voice.SetParameter(AudioParameterId::PitchSemitones, 1.0f);
    voice.SetParameter(AudioParameterId::FormantSemitones, 0.5f);
    voice.SetParameter(AudioParameterId::Wet, 1.0f);

    AudioBuffer voiceBlock(1, 128);
    std::uint64_t absolute = 0;
    for (int block = 0; block < 45; ++block) {
        for (std::size_t frame = 0; frame < voiceBlock.FrameCount(); ++frame) {
            voiceBlock.View().Sample(0, frame) = 0.3f * std::sin(
                2.0 * std::numbers::pi * 220.0 *
                static_cast<double>(absolute++) / format.sampleRate);
        }
        AudioProcessContext context{voiceBlock.View(), voiceBlock.View(), format.sampleRate, absolute - 128};
        voice.Process(context);
    }
    VoiceProcessorMetrics metrics = voice.CaptureMetrics();
    Expect(metrics.state.pitch.voiced &&
        std::fabs(metrics.state.pitch.frequencyHz - 220.0f) < 2.0f,
        "voice chain exposes detected F0 and confidence");
    Expect(Near(metrics.state.targetPitchSemitones, 6.0f, 0.1f) &&
        Near(metrics.state.targetFormantSemitones, -1.0f, 0.1f),
        "voice chain evaluates independent pitch and formant curves");
    Expect(metrics.processedFrames == 45u * 128u && metrics.algorithmLatencyFrames == 128,
        "voice metrics expose frames and algorithm latency");

    Expect(static_cast<bool>(voice.PublishCurves({
        ConstantVoiceCurve(-3.0f),
        ConstantVoiceCurve(2.0f)})), "voice curves update without stream rebuild");
    for (int block = 0; block < 20; ++block) {
        for (std::size_t frame = 0; frame < voiceBlock.FrameCount(); ++frame) {
            voiceBlock.View().Sample(0, frame) = 0.3f * std::sin(
                2.0 * std::numbers::pi * 220.0 *
                static_cast<double>(absolute++) / format.sampleRate);
        }
        AudioProcessContext context{voiceBlock.View(), voiceBlock.View(), format.sampleRate, absolute - 128};
        voice.Process(context);
    }
    metrics = voice.CaptureMetrics();
    Expect(Near(metrics.state.targetPitchSemitones, -2.0f, 0.1f) &&
        Near(metrics.state.targetFormantSemitones, 2.5f, 0.1f),
        "realtime voice state observes newly published curve snapshots");

    voice.SetParameter(AudioParameterId::Bypass, 1.0f);
    for (int block = 0; block < 8; ++block) {
        for (std::size_t frame = 0; frame < voiceBlock.FrameCount(); ++frame) {
            voiceBlock.View().Sample(0, frame) = static_cast<float>(frame) / 128.0f * 0.1f;
        }
        const float expected = voiceBlock.View().Sample(0, 127);
        AudioProcessContext context{voiceBlock.View(), voiceBlock.View(), format.sampleRate, absolute};
        voice.Process(context);
        if (block == 7) {
            Expect(Near(voiceBlock.View().Sample(0, 127), expected, 0.002f),
                "voice bypass crossfades to dry without rebuilding");
        }
    }
    Expect(voice.CaptureMetrics().state.bypassed, "voice realtime bypass state publishes");
}

} // namespace

int main() {
    TestCurve();
    TestAudioBuffer();
    TestParameters();
    TestGraphAndDsp();
    TestPitch();
    TestFormantAndVoice();
    if (gFailures != 0) {
        std::cerr << "Audio runtime tests failed: " << gFailures << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "Audio runtime tests passed\n";
    return EXIT_SUCCESS;
}
