#include "Audio/DSP/Noise/NoiseFrameAdapter.h"
#include "Audio/DSP/Noise/NoiseSuppressorProcessor.h"
#include "Audio/DSP/Noise/RNNoiseSuppressor.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <random>
#include <vector>

namespace {

using namespace Engine::Audio;
int failures = 0;

void Check(bool condition, const char* name) {
    std::cout << (condition ? "[PASS] " : "[FAIL] ") << name << '\n';
    if (!condition) ++failures;
}

class HalfSuppressor final : public INoiseSuppressor {
public:
    AudioStatus Prepare(const NoiseSuppressorPrepareInfo& info) override {
        return info.streamFormat.sampleRate == 48'000
            ? AudioStatus{}
            : AudioStatus{AudioStatusCode::UnsupportedFormat, 0};
    }
    void Reset() noexcept override { ++resets; }
    [[nodiscard]] std::size_t RequiredFrameSize() const noexcept override { return 480; }
    [[nodiscard]] std::uint32_t RequiredSampleRate() const noexcept override { return 48'000; }
    NoiseFrameResult ProcessFrame(
        const float* input, float* output, std::size_t frames) noexcept override {
        if (frames != RequiredFrameSize()) return {0.0f, 0.0f, 0.0, false};
        for (std::size_t index = 0; index < frames; ++index) output[index] = 0.5f * input[index];
        ++calls;
        return {0.75f, 0.0f, 0.0, true};
    }

    std::size_t calls = 0;
    std::size_t resets = 0;
};

bool Near(float left, float right, float tolerance = 1.0e-6f) {
    return std::abs(left - right) <= tolerance;
}

void RunBlockConservationTest(std::size_t blockFrames, const char* name) {
    constexpr std::size_t totalFrames = 24'137;
    constexpr std::size_t latency = 480;
    NoiseFrameAdapter adapter;
    HalfSuppressor suppressor;
    Check(static_cast<bool>(adapter.Prepare(latency)), "FrameAdapter prepare");
    std::vector<float> input(totalFrames);
    std::vector<float> dry(totalFrames);
    std::vector<float> processed(totalFrames);
    bool allBlocksValid = true;
    for (std::size_t index = 0; index < totalFrames; ++index) {
        input[index] = static_cast<float>((index % 997) + 1) / 997.0f;
    }
    for (std::size_t offset = 0; offset < totalFrames; offset += blockFrames) {
        const std::size_t count = std::min(blockFrames, totalFrames - offset);
        const auto result = adapter.Process(
            std::span<const float>(input).subspan(offset, count),
            std::span<float>(dry).subspan(offset, count),
            std::span<float>(processed).subspan(offset, count), suppressor);
        allBlocksValid = allBlocksValid && result.valid;
    }
    bool exact = true;
    for (std::size_t index = 0; index < totalFrames; ++index) {
        const float expectedDry = index < latency ? 0.0f : input[index - latency];
        exact = exact && Near(dry[index], expectedDry) &&
            Near(processed[index], 0.5f * expectedDry);
    }
    const auto metrics = adapter.CaptureMetrics();
    Check(exact, name);
    Check(allBlocksValid, "FrameAdapter continuous block sequence remains valid");
    Check(metrics.inputFrames == totalFrames && metrics.outputFrames == totalFrames,
        "FrameAdapter long-run input/output frame conservation");
    Check(metrics.algorithmFrames == totalFrames / latency &&
        metrics.bufferedInputFrames == totalFrames % latency &&
        metrics.latencyFrames == latency,
        "FrameAdapter algorithm frame and fixed latency accounting");

    adapter.Reset();
    const auto reset = adapter.CaptureMetrics();
    Check(reset.inputFrames == 0 && reset.outputFrames == 0 &&
        reset.algorithmFrames == 0 && reset.bufferedInputFrames == 0,
        "FrameAdapter reset clears FIFO and counters");
    Check(static_cast<bool>(adapter.Prepare(latency)), "FrameAdapter restart prepare");
}

void AdapterTests() {
    RunBlockConservationTest(64, "FrameAdapter 64-frame blocks");
    RunBlockConservationTest(128, "FrameAdapter 128-frame blocks");
    RunBlockConservationTest(256, "FrameAdapter 256-frame blocks");
    RunBlockConservationTest(512, "FrameAdapter 512-frame blocks");
    RunBlockConservationTest(137, "FrameAdapter non-divisible frame combination");

    NoiseSuppressorConfig invalid{true, NoiseSuppressorMode::RNNoise,
        std::numeric_limits<float>::infinity(), -1.0f};
    const auto clamped = ClampNoiseSuppressorConfig(invalid);
    Check(clamped.strength == 1.0f && clamped.wet == 0.0f,
        "Noise config clamps non-finite and out-of-range values");
}

void RNNoiseTests() {
    RNNoiseSuppressor suppressor;
    Check(suppressor.RequiredSampleRate() == 48'000,
        "RNNoise declares required 48 kHz boundary");
    Check(suppressor.Prepare({{44'100, 1, AudioSampleFormat::Float32, 128}}).code ==
        AudioStatusCode::UnsupportedFormat, "RNNoise rejects unsupported 44.1 kHz explicitly");
    Check(static_cast<bool>(suppressor.Prepare(
        {{48'000, 1, AudioSampleFormat::Float32, 128}})),
        "RNNoise initializes from statically linked official model");
    const std::size_t frameSize = suppressor.RequiredFrameSize();
    Check(frameSize == 480, "RNNoise frame size queried from API");

    std::vector<float> input(frameSize, 0.0f);
    std::vector<float> output(frameSize, 1.0f);
    auto result = suppressor.ProcessFrame(input.data(), output.data(), frameSize);
    const float silenceProbability = result.voiceProbability;
    Check(result.valid && std::all_of(output.begin(), output.end(), [](float value) {
        return std::isfinite(value) && std::abs(value) < 1.0e-5f;
    }), "RNNoise silence remains finite and silent");

    std::mt19937 random(42);
    std::uniform_real_distribution<float> noise(-0.12f, 0.12f);
    double inputEnergy = 0.0;
    double outputEnergy = 0.0;
    float lastProbability = 0.0f;
    for (int frame = 0; frame < 80; ++frame) {
        for (float& sample : input) sample = noise(random);
        result = suppressor.ProcessFrame(input.data(), output.data(), frameSize);
        lastProbability = result.voiceProbability;
        if (frame > 10) {
            for (std::size_t index = 0; index < frameSize; ++index) {
                inputEnergy += static_cast<double>(input[index]) * input[index];
                outputEnergy += static_cast<double>(output[index]) * output[index];
            }
        }
    }
    const double attenuation = 10.0 * std::log10((inputEnergy + 1.0e-12) /
        (outputEnergy + 1.0e-12));
    Check(attenuation > 3.0, "RNNoise objectively attenuates generated noise-only signal");
    Check(lastProbability >= 0.0f && lastProbability <= 1.0f,
        "RNNoise exposes bounded VAD probability");

    suppressor.Reset();
    double voicedInputEnergy = 0.0;
    double voicedOutputEnergy = 0.0;
    float maximumVoiceProbability = 0.0f;
    float outputPeak = 0.0f;
    std::uint64_t absoluteFrame = 0;
    for (int frame = 0; frame < 80; ++frame) {
        for (std::size_t index = 0; index < frameSize; ++index) {
            const float t = static_cast<float>(absoluteFrame + index) / 48'000.0f;
            input[index] = 0.16f * std::sin(2.0f * std::numbers::pi_v<float> * 140.0f * t) +
                0.08f * std::sin(2.0f * std::numbers::pi_v<float> * 280.0f * t) +
                0.04f * std::sin(2.0f * std::numbers::pi_v<float> * 420.0f * t) +
                0.02f * noise(random);
        }
        result = suppressor.ProcessFrame(input.data(), output.data(), frameSize);
        maximumVoiceProbability = std::max(maximumVoiceProbability, result.voiceProbability);
        if (frame > 10) {
            for (std::size_t index = 0; index < frameSize; ++index) {
                voicedInputEnergy += static_cast<double>(input[index]) * input[index];
                voicedOutputEnergy += static_cast<double>(output[index]) * output[index];
                outputPeak = std::max(outputPeak, std::abs(output[index]));
            }
        }
        absoluteFrame += frameSize;
    }
    Check(voicedOutputEnergy > voicedInputEnergy * 0.01,
        "RNNoise preserves measurable energy for generated voiced harmonic signal");
    Check(maximumVoiceProbability > silenceProbability + 0.05f,
        "RNNoise VAD probability changes between silence and voiced stimulus");
    Check(outputPeak <= 1.0f && std::isfinite(outputPeak),
        "RNNoise voiced output has finite bounded peak");
    suppressor.Reset();
}

std::vector<float> RunProcessor(
    NoiseSuppressorConfig config,
    std::uint16_t channels = 1,
    std::uint32_t sampleRate = 48'000,
    bool bypassed = false,
    bool injectNonFinite = false) {
    constexpr std::size_t blockFrames = 128;
    constexpr int blocks = 80;
    NoiseSuppressorProcessor processor;
    const AudioFormat format{sampleRate, channels, AudioSampleFormat::Float32, blockFrames};
    Check(static_cast<bool>(processor.Prepare(format)), "Noise processor prepare");
    Check(static_cast<bool>(processor.Publish(BuildNoiseSuppressorSnapshot(config, 1))),
        "Noise processor snapshot publish");
    processor.SetBypassed(bypassed);
    AudioBuffer buffer(channels, blockFrames);
    std::vector<float> captured;
    captured.reserve(blocks * blockFrames * channels);
    std::mt19937 random(7);
    std::uniform_real_distribution<float> noise(-0.08f, 0.08f);
    std::uint64_t absoluteFrame = 0;
    for (int block = 0; block < blocks; ++block) {
        for (std::size_t frame = 0; frame < blockFrames; ++frame) {
            const float tone = 0.12f * std::sin(2.0f * std::numbers::pi_v<float> *
                180.0f * static_cast<float>(absoluteFrame + frame) / sampleRate);
            const float noisySample = tone + noise(random);
            for (std::size_t channel = 0; channel < channels; ++channel) {
                buffer.View().Sample(channel, frame) = noisySample;
            }
        }
        if (injectNonFinite && block == 0) {
            buffer.View().Sample(0, 0) = std::numeric_limits<float>::quiet_NaN();
            buffer.View().Sample(0, 1) = std::numeric_limits<float>::infinity();
        }
        AudioProcessContext context{buffer.View(), buffer.View(), sampleRate, absoluteFrame};
        processor.Process(context);
        for (std::size_t frame = 0; frame < blockFrames; ++frame) {
            for (std::size_t channel = 0; channel < channels; ++channel) {
                captured.push_back(buffer.View().Sample(channel, frame));
            }
        }
        absoluteFrame += blockFrames;
    }
    const auto metrics = processor.CaptureMetrics();
    if (config.enabled && sampleRate == 48'000) {
        Check(metrics.frameSize == 480 && metrics.algorithmicLatencyMilliseconds == 10.0,
            "Noise processor reports queried frame and 10 ms fixed latency");
        Check(metrics.droppedFrames == 0 && metrics.rtAllocations == 0,
            "Noise processor reports no dropped frames or RT allocations");
    }
    return captured;
}

void ProcessorTests() {
    const auto disabled = RunProcessor({false, NoiseSuppressorMode::RNNoise, 1.0f, 1.0f});
    const auto bypassed = RunProcessor({true, NoiseSuppressorMode::RNNoise, 1.0f, 1.0f},
        1, 48'000, true);
    Check(disabled == bypassed, "Disabled and fresh bypass paths equal original signal");

    const auto wetZero = RunProcessor({true, NoiseSuppressorMode::RNNoise, 1.0f, 0.0f});
    Check(disabled == wetZero, "Noise Wet=0 equals original signal");
    const auto wetOne = RunProcessor({true, NoiseSuppressorMode::RNNoise, 1.0f, 1.0f});
    Check(wetOne != disabled && std::all_of(wetOne.begin(), wetOne.end(),
        [](float value) { return std::isfinite(value) && std::abs(value) <= 1.0f; }),
        "Noise Wet=1 uses finite RNNoise output");

    const auto weakest = RunProcessor({true, NoiseSuppressorMode::RNNoise, -3.0f, 1.0f});
    const auto strongest = RunProcessor({true, NoiseSuppressorMode::RNNoise, 9.0f, 1.0f});
    Check(weakest != strongest, "Noise Strength clamp boundaries have real output effect");

    const auto stereo = RunProcessor({true, NoiseSuppressorMode::RNNoise, 1.0f, 1.0f}, 2);
    bool coherentStereo = true;
    for (std::size_t index = 0; index + 1 < stereo.size(); index += 2) {
        coherentStereo = coherentStereo && Near(stereo[index], stereo[index + 1]);
    }
    Check(coherentStereo, "Stereo is coherently downmixed once and restored without image drift");

    const auto unsupported = RunProcessor(
        {true, NoiseSuppressorMode::RNNoise, 1.0f, 1.0f}, 1, 44'100);
    Check(std::all_of(unsupported.begin(), unsupported.end(),
        [](float value) { return std::isfinite(value); }),
        "44.1 kHz boundary is explicit finite pass-through");
    const auto sanitized = RunProcessor(
        {true, NoiseSuppressorMode::RNNoise, 1.0f, 1.0f}, 1, 48'000, false, true);
    Check(std::all_of(sanitized.begin(), sanitized.end(),
        [](float value) { return std::isfinite(value); }), "Noise processor guards NaN/Inf");

    NoiseSuppressorProcessor processor;
    Check(static_cast<bool>(processor.Prepare(
        {48'000, 1, AudioSampleFormat::Float32, 128})), "Noise reset test prepare");
    for (std::uint64_t revision = 1; revision <= 1'000; ++revision) {
        (void)processor.Publish(BuildNoiseSuppressorSnapshot(
            {revision % 2 == 0, NoiseSuppressorMode::RNNoise,
             static_cast<float>(revision % 101) / 100.0f,
             static_cast<float>((revision * 3) % 101) / 100.0f}, revision));
    }
    Check(processor.CaptureMetrics().snapshotRevision == 1'000,
        "1000 Noise parameter snapshots publish with stable latest revision");
    processor.Reset();
    Check(processor.CaptureMetrics().algorithmFrames == 0,
        "Noise processor reset clears DSP state and metrics");
}

} // namespace

int main() {
    AdapterTests();
    RNNoiseTests();
    ProcessorTests();
    std::cout << (failures == 0 ? "Noise suppression tests passed\n"
                                : "Noise suppression tests failed\n");
    return failures == 0 ? 0 : 1;
}
