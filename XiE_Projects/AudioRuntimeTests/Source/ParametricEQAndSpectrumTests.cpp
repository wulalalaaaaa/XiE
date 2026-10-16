#include "Audio/Analysis/SpectrumAnalyzer.h"
#include "Audio/DSP/EQ/ParametricEQ.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <numbers>
#include <random>
#include <thread>

namespace {

using namespace Engine::Audio;
int failures = 0;

void Check(bool condition, const char* name) {
    std::cout << (condition ? "[PASS] " : "[FAIL] ") << name << '\n';
    if (!condition) ++failures;
}

float MeasureGainDb(
    const ParametricEQModel& model,
    float frequency,
    std::uint32_t sampleRate = 48'000,
    std::uint16_t channels = 1) {
    const AudioFormat format{sampleRate, channels, AudioSampleFormat::Float32, 128};
    ParametricEQProcessor processor;
    (void)processor.Prepare(format);
    (void)processor.Publish(BuildParametricEQSnapshot(model, sampleRate, 1));
    AudioBuffer buffer(channels, format.framesPerBuffer);
    double inputSquares = 0.0;
    double outputSquares = 0.0;
    std::uint64_t frameBase = 0;
    for (int block = 0; block < 260; ++block) {
        for (std::size_t frame = 0; frame < format.framesPerBuffer; ++frame) {
            const float sample = 0.1f * std::sin(2.0f * std::numbers::pi_v<float> *
                frequency * static_cast<float>(frameBase + frame) / sampleRate);
            for (std::size_t channel = 0; channel < channels; ++channel) {
                buffer.View().Sample(channel, frame) = sample;
            }
            if (block > 60) inputSquares += static_cast<double>(sample) * sample * channels;
        }
        AudioProcessContext context{buffer.View(), buffer.View(), sampleRate, frameBase};
        processor.Process(context);
        if (block > 60) {
            for (std::size_t channel = 0; channel < channels; ++channel) {
                for (float sample : buffer.View().Channel(channel)) {
                    outputSquares += static_cast<double>(sample) * sample;
                }
            }
        }
        frameBase += format.framesPerBuffer;
    }
    return static_cast<float>(10.0 * std::log10(outputSquares / inputSquares));
}

ParametricEQModel OneBand(EQBandType type, float frequency, float gain, float q = 0.707f) {
    ParametricEQModel model;
    (void)model.AddBand({1, true, type, frequency, gain, q, 12.0f});
    return model;
}

void ModelTests() {
    ParametricEQModel model;
    const auto first = model.AddBand({});
    const auto second = model.AddBand({});
    Check(first && second && *first != *second, "EQ BandId is stable and unique");
    Check(model.RemoveBand(*first) && model.FindBand(*first) == nullptr,
        "EQ band add/remove");
    ParametricEQBand invalid{77, true, EQBandType::Bell, 1.0f, 99.0f, 0.001f, 99.0f};
    Check(model.AddBand(invalid).has_value(), "EQ accepts band through centralized validation");
    const auto* clamped = model.FindBand(77);
    Check(clamped && clamped->frequencyHz == kParametricEQMinimumFrequencyHz &&
        clamped->gainDb == kParametricEQMaximumGainDb &&
        clamped->q == kParametricEQMinimumQ,
        "EQ centralized parameter clamp");
    while (model.Bands().size() < kParametricEQMaxBands) (void)model.AddBand({});
    Check(!model.AddBand({}).has_value(), "EQ maximum 16 bands");
    if (auto* band = model.FindBand(*second)) {
        band->type = EQBandType::Notch;
        Check(model.UpdateBand(*band) && model.FindBand(*second)->type == EQBandType::Notch,
            "EQ band type switch");
    }
}

void ProcessorTests() {
    ParametricEQModel neutral;
    Check(std::abs(MeasureGainDb(neutral, 1'000.0f)) < 0.03f, "empty EQ equals input");
    auto disabled = OneBand(EQBandType::Bell, 1'000.0f, 12.0f);
    disabled.FindBand(1)->enabled = false;
    Check(std::abs(MeasureGainDb(disabled, 1'000.0f)) < 0.03f,
        "disabled band does not affect signal");
    Check(MeasureGainDb(OneBand(EQBandType::Bell, 1'000, 6), 1'000) > 5.2f,
        "Bell boost");
    Check(MeasureGainDb(OneBand(EQBandType::Bell, 1'000, -6), 1'000) < -5.2f,
        "Bell cut");
    Check(MeasureGainDb(OneBand(EQBandType::LowShelf, 400, 6), 80) > 4.0f,
        "Low Shelf");
    Check(MeasureGainDb(OneBand(EQBandType::HighShelf, 4'000, 6), 12'000) > 4.0f,
        "High Shelf");
    Check(MeasureGainDb(OneBand(EQBandType::LowCut, 500, 0), 100) < -15.0f,
        "Low Cut");
    Check(MeasureGainDb(OneBand(EQBandType::HighCut, 2'000, 0), 10'000) < -15.0f,
        "High Cut");
    Check(MeasureGainDb(OneBand(EQBandType::Notch, 1'000, 0, 4.0f), 1'000) < -25.0f,
        "Notch");

    ParametricEQModel multi;
    (void)multi.AddBand({1, true, EQBandType::Bell, 1'000, 6, 1.0f, 12});
    (void)multi.AddBand({2, true, EQBandType::Bell, 1'000, -3, 1.0f, 12});
    const float actual = MeasureGainDb(multi, 1'000);
    Check(actual > 2.2f && actual < 3.8f, "multiple EQ bands add in dB");
    Check(std::abs(MeasureGainDb(neutral, 440, 44'100, 1)) < 0.03f,
        "EQ mono 44.1 kHz");
    Check(std::abs(MeasureGainDb(neutral, 440, 48'000, 2)) < 0.03f,
        "EQ stereo 48 kHz");

    const auto response = EQResponseCalculator::Calculate(multi, 48'000, 512);
    Check(response.frequenciesHz.size() == 512 && response.bandResponseDb.size() == 2,
        "EQ response calculator returns per-band and combined data");

    ParametricEQProcessor processor;
    const AudioFormat format{48'000, 2, AudioSampleFormat::Float32, 128};
    Check(static_cast<bool>(processor.Prepare(format)), "EQ processor prepare");
    bool published = true;
    for (std::uint64_t revision = 1; revision <= 1000; ++revision) {
        ParametricEQModel update = OneBand(EQBandType::Bell,
            100.0f + static_cast<float>(revision % 10'000),
            static_cast<float>(static_cast<int>(revision % 25) - 12));
        published = published && static_cast<bool>(processor.Publish(
            BuildParametricEQSnapshot(update, 48'000, revision)));
    }
    Check(published && processor.CaptureMetrics().snapshotRevision <= 1000,
        "1000 EQ snapshot publications");
    processor.Reset();
    Check(processor.CaptureMetrics().processMilliseconds == 0.0, "EQ reset");
}

std::size_t PeakBin(const std::vector<float>& values) {
    return static_cast<std::size_t>(std::distance(values.begin(),
        std::max_element(values.begin() + 1, values.end())));
}

std::vector<float> Sine(std::size_t size, float frequency, float sampleRate, float amplitude = 1.0f) {
    std::vector<float> samples(size);
    for (std::size_t index = 0; index < size; ++index) {
        samples[index] = amplitude * std::sin(2.0f * std::numbers::pi_v<float> *
            frequency * static_cast<float>(index) / sampleRate);
    }
    return samples;
}

void SpectrumTests() {
    constexpr std::size_t size = 2048;
    constexpr float sampleRate = 48'000.0f;
    const auto hann = SpectrumAnalyzer::BuildHannWindow(size);
    Check(hann.size() == size && std::abs(hann.front()) < 1e-6f &&
        std::abs(hann.back()) < 1e-6f && hann[size / 2] > 0.99f, "Hann window");
    for (float frequency : {100.0f, 440.0f, 1'000.0f}) {
        const auto spectrum = SpectrumAnalyzer::AnalyzeWindow(
            Sine(size, frequency, sampleRate), hann);
        const float detected = static_cast<float>(PeakBin(spectrum)) * sampleRate / size;
        Check(std::abs(detected - frequency) <= sampleRate / size * 1.5f,
            frequency == 100.0f ? "100 Hz spectrum peak" :
            frequency == 440.0f ? "440 Hz spectrum peak" : "1 kHz spectrum peak");
    }
    auto mixedSamples = Sine(size, 440.0f, sampleRate);
    const auto secondTone = Sine(size, 1'000.0f, sampleRate, 0.5f);
    for (std::size_t index = 0; index < size; ++index) mixedSamples[index] += secondTone[index];
    const auto mixedSpectrum = SpectrumAnalyzer::AnalyzeWindow(mixedSamples, hann);
    const auto neighborhoodPeak = [&](float frequency) {
        const std::size_t center = static_cast<std::size_t>(std::lround(frequency * size / sampleRate));
        return *std::max_element(mixedSpectrum.begin() + center - 1,
                                 mixedSpectrum.begin() + center + 2);
    };
    Check(neighborhoodPeak(440.0f) > -3.0f && neighborhoodPeak(1'000.0f) > -9.0f,
        "multiple sine spectrum peaks");
    auto half = SpectrumAnalyzer::AnalyzeWindow(Sine(size, 1'000, sampleRate, 0.5f), hann);
    Check(std::abs(half[PeakBin(half)] + 6.02f) < 0.7f, "spectrum dB conversion");
    std::vector<float> silence(size, 0.0f);
    const auto silent = SpectrumAnalyzer::AnalyzeWindow(silence, hann);
    Check(*std::max_element(silent.begin(), silent.end()) <= -99.9f, "silence spectrum floor");
    std::mt19937 random(42);
    std::uniform_real_distribution<float> noise(-1.0f, 1.0f);
    std::vector<float> white(size);
    for (float& sample : white) sample = noise(random);
    const auto whiteSpectrum = SpectrumAnalyzer::AnalyzeWindow(white, hann);
    Check(whiteSpectrum.size() == size / 2 + 1 &&
        std::count_if(whiteSpectrum.begin(), whiteSpectrum.end(),
            [](float value) { return std::isfinite(value); }) == static_cast<int>(whiteSpectrum.size()),
        "white noise finite spectrum");
    const float attack = SpectrumAnalyzer::SmoothMagnitudeDb(-100, -20, 0.6f, 0.1f);
    const float release = SpectrumAnalyzer::SmoothMagnitudeDb(-20, -100, 0.6f, 0.1f);
    Check(attack > -60.0f && release > -40.0f, "attack/release smoothing");

    SpectrumAnalyzer analyzer;
    const AudioFormat format{48'000, 1, AudioSampleFormat::Float32, 128};
    Check(static_cast<bool>(analyzer.Prepare(format)), "spectrum analyzer prepare");
    AudioBuffer input(1, 128);
    AudioBuffer output(1, 128);
    std::uint64_t frameBase = 0;
    for (int block = 0; block < 80; ++block) {
        for (std::size_t frame = 0; frame < 128; ++frame) {
            input.View().Sample(0, frame) = std::sin(2.0f * std::numbers::pi_v<float> *
                440.0f * static_cast<float>(frameBase + frame) / 48'000.0f);
            output.View().Sample(0, frame) = 0.5f * input.View().Sample(0, frame);
        }
        const AudioBuffer& constInput = input;
        const AudioBuffer& constOutput = output;
        analyzer.PushInput(constInput.View());
        analyzer.PushOutput(constOutput.View());
        frameBase += 128;
    }
    std::shared_ptr<const SpectrumSnapshot> snapshot;
    for (int wait = 0; wait < 100 && !snapshot; ++wait) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        snapshot = analyzer.CaptureSnapshot();
    }
    Check(snapshot && !snapshot->inputMagnitudeDb.empty() &&
        snapshot->inputMagnitudeDb.size() == snapshot->outputMagnitudeDb.size(),
        "input/output spectrum snapshot");
    Check(analyzer.CaptureMetrics().revision > 0, "spectrum latest-only revision publication");
}

} // namespace

int main() {
    ModelTests();
    ProcessorTests();
    SpectrumTests();
    std::cout << (failures == 0 ? "Parametric EQ and spectrum tests passed\n"
                                : "Parametric EQ and spectrum tests failed\n");
    return failures == 0 ? 0 : 1;
}
