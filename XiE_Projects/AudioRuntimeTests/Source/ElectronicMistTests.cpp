#include "Audio/DSP/ElectronicMist/ElectronicMist.h"
#include "Audio/Voice/VoiceProcessor.h"
#include "Curve/Curve.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <utility>

namespace {

bool Check(bool condition, const char* message) {
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

void FillTone(Engine::Audio::AudioBuffer& buffer, std::size_t offset,
    std::uint32_t sampleRate, float frequency = 440.0f, float amplitude = 0.2f) {
    for (std::size_t frame = 0; frame < buffer.FrameCount(); ++frame) {
        const float sample = amplitude * std::sin(
            2.0 * 3.14159265358979323846 * frequency *
            static_cast<double>(frame + offset) / static_cast<double>(sampleRate));
        for (std::size_t channel = 0; channel < buffer.ChannelCount(); ++channel) {
            buffer.View().Sample(channel, frame) = sample;
        }
    }
}

float Rms(const Engine::Audio::AudioBuffer& buffer) {
    double sum = 0.0;
    std::size_t count = 0;
    for (std::size_t channel = 0; channel < buffer.ChannelCount(); ++channel) {
        for (const float sample : buffer.View().Channel(channel)) {
            sum += static_cast<double>(sample) * sample;
            ++count;
        }
    }
    return count == 0 ? 0.0f : static_cast<float>(std::sqrt(sum / count));
}

bool TestGenerator() {
    using namespace Engine::Audio;
    ElectronicMistNoiseGenerator first(42);
    ElectronicMistNoiseGenerator second(42);
    first.Prepare(2);
    second.Prepare(2);
    bool nonZero = false;
    for (int index = 0; index < 128; ++index) {
        const float a = first.Next(0, ElectronicMistNoiseType::Digital);
        const float b = second.Next(0, ElectronicMistNoiseType::Digital);
        if (!std::isfinite(a) || std::abs(a) > 2.0f) return false;
        nonZero = nonZero || std::abs(a) > 1.0e-6f;
    }
    if (!Check(nonZero, "noise generator sequence advances")) return false;
    first.Reset();
    second.Reset();
    for (int index = 0; index < 8; ++index) {
        if (!Check(first.Next(0, ElectronicMistNoiseType::White) ==
            second.Next(0, ElectronicMistNoiseType::White), "seed reset is reproducible")) return false;
    }
    return true;
}

bool TestProcessor(const Engine::Audio::AudioFormat& format) {
    using namespace Engine::Audio;
    ElectronicMistProcessor processor(7);
    if (!Check(static_cast<bool>(processor.Prepare(format)), "Electronic Mist prepare")) return false;
    AudioBuffer buffer(format.channelCount, format.framesPerBuffer);
    FillTone(buffer, 0, format.sampleRate);
    AudioBuffer original = buffer;
    AudioProcessContext context{buffer.View(), buffer.View(), format.sampleRate, 0};
    processor.Process(context);
    if (!Check(Rms(buffer) == Rms(original), "disabled mist is dry")) return false;

    Engine::Curve::CurveModel curve;
    (void)curve.AddPoint({1, 20.0f, 1.0f, Engine::Curve::CurveInterpolation::Smooth});
    (void)curve.AddPoint({2, 20'000.0f, 1.0f, Engine::Curve::CurveInterpolation::Smooth});
    auto curveSnapshot = Engine::Curve::BuildCurveSnapshot(curve, 1024, 20.0f, 20'000.0f);
    ElectronicMistConfig config;
    config.enabled = true;
    config.amount = 0.55f;
    config.follow = 0.80f;
    config.sharpness = 0.60f;
    config.width = format.channelCount > 1 ? 0.75f : 0.0f;
    config.wet = 1.0f;
    if (!Check(static_cast<bool>(processor.Publish(BuildElectronicMistSnapshot(
        config, std::move(curveSnapshot), 3))), "Electronic Mist publish")) return false;

    bool changed = false;
    for (int block = 0; block < 80; ++block) {
        FillTone(buffer, static_cast<std::size_t>(block) * buffer.FrameCount(), format.sampleRate);
        original = buffer;
        context = {buffer.View(), buffer.View(), format.sampleRate,
            static_cast<std::uint64_t>(block) * buffer.FrameCount()};
        processor.Process(context);
        for (std::size_t channel = 0; channel < buffer.ChannelCount(); ++channel) {
            for (std::size_t frame = 0; frame < buffer.FrameCount(); ++frame) {
                changed = changed || std::abs(buffer.View().Sample(channel, frame) -
                    original.View().Sample(channel, frame)) > 1.0e-7f;
                if (!std::isfinite(buffer.View().Sample(channel, frame))) return false;
            }
        }
    }
    const ElectronicMistMetrics metrics = processor.CaptureMetrics();
    if (!Check(changed, "voice-following mist changes a tone")) return false;
    if (!Check(metrics.enabled && metrics.snapshotRevision == 3 && metrics.bandCount == 8,
               "mist metrics and revision")) return false;

    // Snapshot publication is deliberately exercised off the audio callback.  The
    // processor must keep the audio-side pointer stable while the control side
    // replaces immutable snapshots repeatedly.
    for (std::uint64_t revision = 4; revision < 1004; ++revision) {
        if (!Check(static_cast<bool>(processor.Publish(BuildElectronicMistSnapshot(
            config, {}, revision))), "1000 mist snapshot publications")) return false;
    }
    processor.Process(context);
    if (!Check(processor.CaptureMetrics().snapshotRevision == 1003,
               "mist snapshot stress revision")) return false;

    buffer.Clear();
    float silenceMaximum = 0.0f;
    for (int block = 0; block < 400; ++block) {
        buffer.Clear();
        context = {buffer.View(), buffer.View(), format.sampleRate,
            static_cast<std::uint64_t>(1000 + block) * buffer.FrameCount()};
        processor.Process(context);
        if (block >= 320) silenceMaximum = std::max(silenceMaximum, Rms(buffer));
    }
    if (!Check(silenceMaximum < 1.0e-5f, "silence does not sustain mist")) return false;

    config.amount = 0.0f;
    if (!Check(static_cast<bool>(processor.Publish(BuildElectronicMistSnapshot(
        config, {}, 1004))), "amount zero publish")) return false;
    FillTone(buffer, 0, format.sampleRate);
    original = buffer;
    context = {buffer.View(), buffer.View(), format.sampleRate, 20'000};
    processor.Process(context);
    if (!Check(Rms(buffer) == Rms(original), "amount zero is dry")) return false;
    processor.Reset();
    return Check(processor.CaptureMetrics().processedFrames == 0, "mist reset");
}

} // namespace

int main() {
    using namespace Engine::Audio;
    if (!TestGenerator()) return EXIT_FAILURE;
    for (const AudioFormat format : {
        AudioFormat{44'100, 1, AudioSampleFormat::Float32, 128},
        AudioFormat{48'000, 2, AudioSampleFormat::Float32, 128}}) {
        if (!TestProcessor(format)) return EXIT_FAILURE;
    }
    std::cout << "Electronic Mist: deterministic noise, voice-following bands, silence gate, "
                 "mono/stereo 44.1/48 kHz, snapshot/reset checks passed\n";
    return EXIT_SUCCESS;
}
