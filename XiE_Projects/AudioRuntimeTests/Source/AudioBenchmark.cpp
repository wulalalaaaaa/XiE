#include "Audio/DSP/BasicProcessors.h"
#include "Audio/Graph/AudioGraph.h"
#include "Audio/Voice/VoiceProcessor.h"
#include "Curve/Curve.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <new>
#include <numbers>
#include <string_view>
#include <vector>

namespace {
std::atomic<std::uint64_t> gAllocations{0};
thread_local bool gCountAllocations = false;
}

void* operator new(std::size_t size) {
    if (gCountAllocations) {
        gAllocations.fetch_add(1, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) {
        return memory;
    }
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace {

using Clock = std::chrono::steady_clock;
using namespace Engine::Audio;

struct Result {
    double averageMs = 0.0;
    double p95Ms = 0.0;
    double p99Ms = 0.0;
    double maximumMs = 0.0;
    double cpuPercent = 0.0;
    std::uint64_t allocations = 0;
    std::uint64_t underruns = 0;
    double noiseAverageMs = 0.0;
    double noiseP95Ms = 0.0;
    double noiseP99Ms = 0.0;
    double noiseMaximumMs = 0.0;
    double noiseLatencyMs = 0.0;
    double softHazeMs = 0.0;
    double spatialHazeMs = 0.0;
    double spatialHazeTailMs = 0.0;
    double syntheticMs = 0.0;
    double microPitchMs = 0.0;
    double syntheticLatencyMs = 0.0;
    double electronicMistMs = 0.0;
    double electronicMistRatioDb = 0.0;
};

template<class Callback>
Result Measure(const AudioFormat& format, Callback&& callback) {
    constexpr std::size_t iterations = 1000;
    std::vector<double> durations;
    durations.reserve(iterations);
    for (int warmup = 0; warmup < 50; ++warmup) {
        callback();
    }

    gAllocations.store(0, std::memory_order_release);
    gCountAllocations = true;
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
        const auto begin = Clock::now();
        callback();
        const auto end = Clock::now();
        durations.push_back(std::chrono::duration<double, std::milli>(end - begin).count());
    }
    gCountAllocations = false;
    std::sort(durations.begin(), durations.end());

    Result result;
    for (double duration : durations) {
        result.averageMs += duration;
    }
    result.averageMs /= static_cast<double>(durations.size());
    result.p95Ms = durations[static_cast<std::size_t>(durations.size() * 0.95)];
    result.p99Ms = durations[static_cast<std::size_t>(durations.size() * 0.99)];
    result.maximumMs = durations.back();
    const double deadline =
        1000.0 * static_cast<double>(format.framesPerBuffer) / format.sampleRate;
    result.cpuPercent = result.averageMs / deadline * 100.0;
    result.allocations = gAllocations.load(std::memory_order_acquire);
    result.underruns = static_cast<std::uint64_t>(std::count_if(
        durations.begin(), durations.end(), [deadline](double value) { return value >= deadline; }));
    return result;
}

void Print(
    std::string_view graph,
    const AudioFormat& format,
    const Result& result) {
    std::cout << graph << ',' << format.sampleRate << ',' << format.channelCount << ','
              << format.framesPerBuffer << ',' << std::fixed << std::setprecision(4)
              << result.averageMs << ',' << result.p95Ms << ',' << result.p99Ms << ','
              << result.maximumMs << ',' << result.cpuPercent << ','
              << result.allocations << ',' << result.underruns << ','
              << result.noiseAverageMs << ',' << result.noiseP95Ms << ','
              << result.noiseP99Ms << ',' << result.noiseMaximumMs << ','
              << result.noiseLatencyMs << ',' << result.softHazeMs << ','
              << result.spatialHazeMs << ',' << result.spatialHazeTailMs << ','
              << result.syntheticMs << ',' << result.microPitchMs << ','
              << result.syntheticLatencyMs << ',' << result.electronicMistMs << ','
              << result.electronicMistRatioDb << '\n';
}

void RunCase(const AudioFormat& format) {
    AudioBuffer input(format.channelCount, format.framesPerBuffer);
    AudioBuffer output(format.channelCount, format.framesPerBuffer);
    for (std::size_t channel = 0; channel < input.ChannelCount(); ++channel) {
        for (std::size_t frame = 0; frame < input.FrameCount(); ++frame) {
            input.View().Sample(channel, frame) = 0.25f * std::sin(
                2.0 * std::numbers::pi * 220.0 * static_cast<double>(frame) / format.sampleRate);
        }
    }
    AudioProcessContext context{input.View(), output.View(), format.sampleRate, 0};

    AudioGraph emptyGraph;
    emptyGraph.Prepare(format);
    AudioGraphBuilder emptyBuilder;
    emptyGraph.Publish(emptyBuilder.Build(format));
    Print("empty", format, Measure(format, [&] { emptyGraph.Process(context); }));

    AudioGraph basicGraph;
    basicGraph.Prepare(format);
    AudioGraphBuilder basicBuilder;
    basicBuilder.Add(std::make_unique<GainProcessor>(0.9f));
    basicBuilder.Add(std::make_unique<DCBlocker>());
    basicBuilder.Add(std::make_unique<Limiter>());
    basicGraph.Publish(basicBuilder.Build(format));
    Print("basic", format, Measure(format, [&] { basicGraph.Process(context); }));

    const auto runVoice = [&](std::string_view name, std::size_t bands, bool analyzer,
                              bool noiseEnabled, bool pitch, bool formant,
                              bool softHaze, bool spatialHaze,
                              SyntheticVoiceConfig syntheticVoice = {},
                              bool electronicMist = false) {
        VoiceProcessorConfig config;
        config.format = format;
        config.pitchShiftWindowFrames = 256;
        config.analyzer.enabled = analyzer;
        config.noiseSuppression.enabled = noiseEnabled;
        config.softHaze.enabled = softHaze;
        config.spatialHaze.enabled = spatialHaze;
        config.syntheticVoice = syntheticVoice;
        config.electronicMist.enabled = electronicMist;
        config.electronicMist.amount = electronicMist ? 0.25f : 0.0f;
        config.electronicMist.follow = 0.80f;
        config.electronicMist.sharpness = 0.50f;
        config.electronicMist.width = format.channelCount > 1 ? 0.35f : 0.0f;
        config.electronicMist.wet = 1.0f;
        for (std::size_t band = 0; band < bands; ++band) {
            (void)config.parametricEQ.AddBand({0, true, EQBandType::Bell,
                80.0f * std::pow(1.42f, static_cast<float>(band)),
                band % 2 == 0 ? 2.0f : -2.0f,
                0.7f + static_cast<float>(band % 4) * 0.3f, 12.0f});
        }
        VoiceProcessor voice;
        voice.Prepare(config);
        if (electronicMist) {
            Engine::Curve::CurveModel mistCurve;
            (void)mistCurve.AddPoint({1, 20.0f, 1.0f,
                Engine::Curve::CurveInterpolation::Smooth});
            (void)mistCurve.AddPoint({2, 20'000.0f, 1.0f,
                Engine::Curve::CurveInterpolation::Smooth});
            (void)voice.PublishElectronicMist(BuildElectronicMistSnapshot(
                config.electronicMist,
                Engine::Curve::BuildCurveSnapshot(mistCurve, 1024, 20.0f, 20'000.0f), 1));
        }
        voice.SetPitchDetectorBypassed(!pitch && !formant);
        voice.SetPitchShiftBypassed(!pitch);
        voice.SetFormantBypassed(!formant);
        voice.SetEQBypassed(bands == 0);
        AudioProcessContext voiceContext{input.View(), output.View(), format.sampleRate, 0};
        Result result = Measure(format, [&] {
            std::copy(input.View().Data(),
                input.View().Data() + input.ChannelCount() * input.FrameCount(),
                output.View().Data());
            voice.Process(voiceContext);
        });
        const auto noise = voice.CaptureMetrics().noise;
        result.noiseAverageMs = noise.averageProcessMilliseconds;
        result.noiseP95Ms = noise.p95ProcessMilliseconds;
        result.noiseP99Ms = noise.p99ProcessMilliseconds;
        result.noiseMaximumMs = noise.maximumProcessMilliseconds;
        result.noiseLatencyMs = noise.algorithmicLatencyMilliseconds;
        const auto metrics = voice.CaptureMetrics();
        result.softHazeMs = metrics.softHaze.processMilliseconds;
        result.spatialHazeMs = metrics.spatialHaze.processMilliseconds;
        result.spatialHazeTailMs = metrics.spatialHaze.effectiveTailMilliseconds;
        result.syntheticMs = metrics.syntheticVoice.processMilliseconds;
        result.microPitchMs = metrics.syntheticVoice.micro.processMilliseconds;
        result.syntheticLatencyMs = 1000.0 * static_cast<double>(
            metrics.syntheticVoice.algorithmicLatencyFrames) / format.sampleRate;
        result.electronicMistMs = metrics.electronicMist.processMilliseconds;
        result.electronicMistRatioDb = metrics.electronicMist.mistToVoiceDb;
        Print(name, format, result);
    };
    runVoice("voice_without_noise_eq_analyzer", 0, false, false, false, false, false, false);
    runVoice("voice_eq1", 1, false, false, false, false, false, false);
    runVoice("voice_eq8", 8, false, false, false, false, false, false);
    runVoice("voice_eq16", 16, false, false, false, false, false, false);
    runVoice("voice_soft_haze1", 0, false, false, false, false, true, false);
    runVoice("voice_spatial_haze1", 0, false, false, false, false, false, true);
    runVoice("voice_soft_spatial_haze", 0, false, false, false, false, true, true);
    runVoice("voice_synthetic_stabilization", 0, false, false, true, false, false, false,
        {true, 0.55f, 0.85f, 0.0f, 0.5f, 1.0f});
    runVoice("voice_synthetic_micro", 0, false, false, false, false, false, false,
        {true, 0.55f, 0.0f, 0.75f, 0.65f, 1.0f});
    runVoice("voice_synthetic_with_haze", 8, true, true, true, true, true, true,
        {true, 0.45f, 0.65f, 0.55f, 0.55f, 0.8f});
    runVoice("voice_mist8", 0, false, false, false, false, false, false, {}, true);
    runVoice("voice_mist_synthetic_haze", 8, true, true, true, true, true, true,
        {true, 0.45f, 0.65f, 0.55f, 0.55f, 0.8f}, true);
    runVoice("voice_rnnoise_pitch_formant_eq16_mist_analyzer_haze", 16, true, true, true, true, true, true,
        {}, true);
}

} // namespace

int main() {
    std::cout << "graph,sample_rate,channels,frames,avg_ms,p95_ms,p99_ms,max_ms,cpu_percent,"
                 "allocations,deadline_misses,noise_avg_ms,noise_p95_ms,noise_p99_ms,"
                 "noise_max_ms,noise_latency_ms,soft_haze_ms,spatial_haze_ms,"
                 "spatial_haze_tail_ms,synthetic_ms,micro_pitch_ms,synthetic_latency_ms,"
                 "electronic_mist_ms,electronic_mist_ratio_db\n";
    for (std::uint16_t channels : {std::uint16_t{1}, std::uint16_t{2}}) {
        for (std::uint32_t frames : {64u, 128u, 256u, 512u}) {
            RunCase({48'000, channels, AudioSampleFormat::Float32, frames});
        }
    }
    return EXIT_SUCCESS;
}
