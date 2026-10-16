#include "Audio/DSP/BasicProcessors.h"
#include "Audio/Graph/AudioGraph.h"
#include "Audio/Voice/VoiceProcessor.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>
#include <thread>

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

void* operator new[](std::size_t size) {
    return ::operator new(size);
}

void operator delete(void* memory) noexcept {
    std::free(memory);
}

void operator delete[](void* memory) noexcept {
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
    std::free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept {
    std::free(memory);
}

int main() {
    using namespace Engine::Audio;
    static_assert(noexcept(std::declval<AudioGraph&>().Process(
        std::declval<AudioProcessContext&>())));
    static_assert(std::atomic<float>::is_always_lock_free);
    static_assert(std::atomic<double>::is_always_lock_free);
    static_assert(std::atomic<AudioGraphSnapshot*>::is_always_lock_free);

    const AudioFormat format{48'000, 2, AudioSampleFormat::Float32, 128};
    AudioGraphBuilder builder;
    builder.Add(std::make_unique<GainProcessor>(0.8f));
    builder.Add(std::make_unique<DCBlocker>());
    builder.Add(std::make_unique<Limiter>());
    auto snapshot = builder.Build(format);
    AudioGraph graph;
    graph.Prepare(format);
    graph.Publish(std::move(snapshot));

    VoiceProcessorConfig voiceConfig;
    voiceConfig.format = format;
    voiceConfig.pitchShiftWindowFrames = 256;
    voiceConfig.noiseSuppression.enabled = true;
    voiceConfig.softHaze.enabled = true;
    voiceConfig.spatialHaze.enabled = true;
    voiceConfig.syntheticVoice = {true, 0.55f, 0.75f, 0.45f, 0.6f, 0.8f};
    VoiceProcessor voice;
    voice.Prepare(voiceConfig);

    AudioBuffer input(2, 128);
    AudioBuffer output(2, 128);
    AudioProcessContext graphContext{input.View(), output.View(), format.sampleRate, 0};
    AudioProcessContext voiceContext{output.View(), output.View(), format.sampleRate, 0};

    for (int warmup = 0; warmup < 20; ++warmup) {
        graph.Process(graphContext);
        voice.Process(voiceContext);
    }
    gAllocations.store(0, std::memory_order_release);
    std::atomic<bool> publisherDone{false};
    std::thread publisher([&] {
        for (int update = 0; update < 1000; ++update) {
            Engine::Curve::CurveModel pitchModel;
            Engine::Curve::CurveModel formantModel;
            const float pitch = static_cast<float>((update % 25) - 12);
            const float formant = static_cast<float>((update % 13) - 6);
            (void)pitchModel.AddPoint({1, 70, pitch, Engine::Curve::CurveInterpolation::Smooth});
            (void)pitchModel.AddPoint({2, 500, -pitch, Engine::Curve::CurveInterpolation::Smooth});
            (void)formantModel.AddPoint({1, 70, formant, Engine::Curve::CurveInterpolation::Smooth});
            (void)formantModel.AddPoint({2, 500, -formant, Engine::Curve::CurveInterpolation::Smooth});
            VoiceCurveSet curves{
                Engine::Curve::BuildCurveSnapshot(pitchModel, 1024, 70, 500),
                Engine::Curve::BuildCurveSnapshot(formantModel, 1024, 70, 500)};
            (void)voice.PublishCurves(curves);
            ParametricEQModel eq;
            (void)eq.AddBand({1, true, EQBandType::Bell,
                100.0f + static_cast<float>(update % 10'000),
                static_cast<float>((update % 25) - 12), 0.7f + (update % 10) * 0.1f, 12.0f});
            (void)voice.PublishParametricEQ(
                BuildParametricEQSnapshot(eq, format.sampleRate,
                    static_cast<std::uint64_t>(update + 1)));
            NoiseSuppressorConfig noise;
            noise.enabled = update % 7 != 0;
            noise.strength = static_cast<float>(update % 101) / 100.0f;
            noise.wet = static_cast<float>((update * 3) % 101) / 100.0f;
            (void)voice.PublishNoiseSuppression(
                BuildNoiseSuppressorSnapshot(noise,
                    static_cast<std::uint64_t>(update + 1)));
            SoftHazeConfig softHaze;
            softHaze.enabled = update % 5 != 0;
            softHaze.amount = static_cast<float>((update * 7) % 101) / 100.0f;
            softHaze.blur = static_cast<float>((update * 11) % 101) / 100.0f;
            softHaze.damping = static_cast<float>((update * 13) % 101) / 100.0f;
            softHaze.wet = static_cast<float>((update * 17) % 101) / 100.0f;
            (void)voice.PublishSoftHaze(
                BuildSoftHazeSnapshot(softHaze,
                    static_cast<std::uint64_t>(update + 1)));
            SpatialHazeConfig spatialHaze;
            spatialHaze.enabled = update % 7 != 0;
            spatialHaze.amount = static_cast<float>((update * 3) % 101) / 100.0f;
            spatialHaze.size = static_cast<float>((update * 5) % 101) / 100.0f;
            spatialHaze.diffusion = static_cast<float>((update * 19) % 101) / 100.0f;
            spatialHaze.damping = static_cast<float>((update * 23) % 101) / 100.0f;
            spatialHaze.width = static_cast<float>((update * 29) % 101) / 100.0f;
            spatialHaze.wet = static_cast<float>((update * 31) % 101) / 100.0f;
            (void)voice.PublishSpatialHaze(
                BuildSpatialHazeSnapshot(spatialHaze,
                    static_cast<std::uint64_t>(update + 1)));
            SyntheticVoiceConfig synthetic;
            synthetic.enabled = update % 5 != 0;
            synthetic.amount = static_cast<float>((update * 5) % 101) / 100.0f;
            synthetic.stability = static_cast<float>((update * 7) % 101) / 100.0f;
            synthetic.electronic = static_cast<float>((update * 11) % 101) / 100.0f;
            synthetic.width = static_cast<float>((update * 13) % 101) / 100.0f;
            synthetic.wet = static_cast<float>((update * 17) % 101) / 100.0f;
            (void)voice.PublishSyntheticVoice(
                BuildSyntheticVoiceSnapshot(synthetic,
                    static_cast<std::uint64_t>(update + 1)));
            ElectronicMistConfig electronicMist;
            electronicMist.enabled = false;
            electronicMist.amount = static_cast<float>((update * 3) % 101) / 100.0f;
            electronicMist.follow = static_cast<float>((update * 5) % 101) / 100.0f;
            electronicMist.sharpness = static_cast<float>((update * 7) % 101) / 100.0f;
            electronicMist.width = static_cast<float>((update * 11) % 101) / 100.0f;
            electronicMist.wet = static_cast<float>((update * 13) % 101) / 100.0f;
            (void)voice.PublishElectronicMist(BuildElectronicMistSnapshot(
                electronicMist, static_cast<std::uint64_t>(update + 1)));
            voice.SetNoiseBypassed((update & 1) != 0);
            voice.SetParameter(AudioParameterId::Wet, static_cast<float>(update % 101) / 100.0f);
            voice.SetParameter(AudioParameterId::Bypass, (update & 1) ? 1.0f : 0.0f);
        }
        publisherDone.store(true, std::memory_order_release);
    });

    double maximumMilliseconds = 0.0;
    std::uint64_t deadlineMisses = 0;
    int iteration = 0;
    gCountAllocations = true;
    while (iteration < 2000 || !publisherDone.load(std::memory_order_acquire)) {
        const auto begin = std::chrono::steady_clock::now();
        graph.Process(graphContext);
        voice.Process(voiceContext);
        const double elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - begin).count();
        maximumMilliseconds = std::max(maximumMilliseconds, elapsed);
        if (elapsed > 1000.0 * format.framesPerBuffer / format.sampleRate) {
            ++deadlineMisses;
        }
        ++iteration;
    }
    gCountAllocations = false;
    publisher.join();

    const std::uint64_t allocations = gAllocations.load(std::memory_order_acquire);
    if (allocations != 0) {
        std::cerr << "RT Process path allocated " << allocations << " times\n";
        return EXIT_FAILURE;
    }
    if (deadlineMisses != 0) {
        std::cerr << "RT Process path missed " << deadlineMisses << " deadlines\n";
        return EXIT_FAILURE;
    }
    const auto noiseMetrics = voice.CaptureMetrics().noise;
    std::cout << "RT Process paths: 0 allocations; noexcept; lock-free atomics; "
              << "concurrent curve/parameter/EQ/noise/soft-haze/spatial-haze/synthetic/electronic-mist publications=1000; "
              << "noise_enable_and_bypass_updates=1000; max_ms="
              << maximumMilliseconds << "; deadline_misses=" << deadlineMisses
              << "; noise_rt_allocations=" << noiseMetrics.rtAllocations << '\n';
    return EXIT_SUCCESS;
}
