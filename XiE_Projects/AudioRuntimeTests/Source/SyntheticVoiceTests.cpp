#include "Audio/DSP/Synthetic/MicroPitchProcessor.h"
#include "Audio/DSP/Synthetic/PitchStabilizer.h"
#include "Audio/DSP/Synthetic/SyntheticVoice.h"
#include "Audio/Voice/VoiceProcessor.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

bool Check(bool condition, const char* message) {
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

bool Finite(const Engine::Audio::AudioBuffer& buffer) {
    for (std::size_t channel = 0; channel < buffer.ChannelCount(); ++channel) {
        for (const float sample : buffer.View().Channel(channel)) {
            if (!std::isfinite(sample)) return false;
        }
    }
    return true;
}

void FillTone(Engine::Audio::AudioBuffer& buffer, std::size_t offset,
    std::uint32_t sampleRate, float frequency = 220.0f) {
    for (std::size_t frame = 0; frame < buffer.FrameCount(); ++frame) {
        const float sample = 0.2f * std::sin(
            2.0 * 3.14159265358979323846 * frequency *
            static_cast<double>(frame + offset) / static_cast<double>(sampleRate));
        for (std::size_t channel = 0; channel < buffer.ChannelCount(); ++channel) {
            buffer.View().Sample(channel, frame) = sample;
        }
    }
}

bool TestStabilizer() {
    using namespace Engine::Audio;
    PitchStabilizer stabilizer;
    const AudioFormat format{48'000, 1, AudioSampleFormat::Float32, 128};
    stabilizer.Prepare(format);
    stabilizer.SetConfig({true, 0.85f, 0.70f, 12.0f, 0.60f, 40.0f});

    float maximumCorrection = 0.0f;
    float first = 0.0f;
    float last = 0.0f;
    for (int index = 0; index < 80; ++index) {
        const PitchEstimate estimate{220.0f + (index % 2 == 0 ? 0.7f : -0.7f), 0.95f, true};
        const auto result = stabilizer.Process(estimate, 128);
        if (index == 0) first = result.stabilizedFrequencyHz;
        last = result.stabilizedFrequencyHz;
        maximumCorrection = std::max(maximumCorrection, std::abs(result.correctionCents));
    }
    if (!Check(std::abs(last - 220.0f) < 1.0f, "stabilizer stable 220 Hz")) return false;
    if (!Check(maximumCorrection <= 12.001f, "stabilizer correction clamp")) return false;
    if (!Check(std::abs(last - first) < 1.5f, "stabilizer jitter suppression")) return false;

    const auto ramp = stabilizer.Process({330.0f, 0.95f, true}, 128);
    if (!Check(ramp.stabilizedFrequencyHz > 220.0f, "stabilizer preserves pitch movement")) return false;
    if (!Check(std::abs(ramp.correctionCents) <= 12.001f, "ramp correction bounded")) return false;
    const auto lowConfidence = stabilizer.Process({0.0f, 0.0f, false}, 128);
    if (!Check(std::abs(lowConfidence.correctionCents) <=
            std::abs(ramp.correctionCents) + 0.01f,
               "confidence gate does not jump")) return false;
    stabilizer.SetConfig({true, 0.0f, 0.7f, 12.0f, 0.6f, 40.0f});
    const auto natural = stabilizer.Process({440.0f, 0.95f, true}, 128);
    if (!Check(std::abs(natural.correctionCents) < 1.0e-4f &&
               std::abs(natural.stabilizedFrequencyHz - 440.0f) < 1.0e-4f,
               "stability zero is natural")) return false;
    stabilizer.Reset();
    return Check(stabilizer.Latest().stabilizedFrequencyHz == 0.0f, "stabilizer reset");
}

bool TestMicroPitch(const Engine::Audio::AudioFormat& format) {
    using namespace Engine::Audio;
    MicroPitchProcessor processor(128);
    if (!Check(static_cast<bool>(processor.Prepare(format)), "micro pitch prepare")) return false;
    AudioBuffer buffer(format.channelCount, format.framesPerBuffer);
    FillTone(buffer, 0, format.sampleRate);
    AudioBuffer original = buffer;
    AudioProcessContext context{buffer.View(), buffer.View(), format.sampleRate, 0};
    processor.Process(context);
    for (std::size_t channel = 0; channel < buffer.ChannelCount(); ++channel) {
        for (std::size_t frame = 0; frame < buffer.FrameCount(); ++frame) {
            if (!Check(buffer.View().Sample(channel, frame) ==
                original.View().Sample(channel, frame), "micro amount zero is dry")) return false;
        }
    }
    processor.SetConfig({true, 0.75f, 0.8f, 0.0f, 0.65f}, 7);
    bool changed = false;
    for (int block = 0; block < 80; ++block) {
        FillTone(buffer, static_cast<std::size_t>(block) * buffer.FrameCount(), format.sampleRate);
        original = buffer;
        context = {buffer.View(), buffer.View(), format.sampleRate,
            static_cast<std::uint64_t>(block) * buffer.FrameCount()};
        processor.Process(context);
        if (!Check(Finite(buffer), "micro pitch finite")) return false;
        for (std::size_t channel = 0; channel < buffer.ChannelCount(); ++channel) {
            for (std::size_t frame = 0; frame < buffer.FrameCount(); ++frame) {
                changed = changed || std::abs(buffer.View().Sample(channel, frame) -
                    original.View().Sample(channel, frame)) > 1.0e-6f;
            }
        }
    }
    const auto metrics = processor.CaptureMetrics();
    if (!Check(changed && metrics.snapshotRevision == 7, "micro layer changes signal")) return false;
    if (!Check(metrics.detuneCents > 0.0f && metrics.delaySamples > 0,
               "micro detune and delay metrics")) return false;
    processor.SetConfig({true, 1.0f, 0.5f, 0.0f, 0.0f}, 8);
    for (int block = 0; block < 12; ++block) {
        FillTone(buffer, static_cast<std::size_t>(1000 + block) * buffer.FrameCount(), format.sampleRate);
        original = buffer;
        context = {buffer.View(), buffer.View(), format.sampleRate,
            static_cast<std::uint64_t>(1000 + block) * buffer.FrameCount()};
        processor.Process(context);
    }
    for (std::size_t frame = 0; frame < buffer.FrameCount(); ++frame) {
        if (!Check(std::abs(buffer.View().Sample(0, frame) -
            original.View().Sample(0, frame)) < 1.0e-5f, "micro wet zero is dry")) return false;
    }
    processor.Reset();
    return Check(processor.CaptureMetrics().processedFrames == 0, "micro reset");
}

bool TestVoiceIntegration(const Engine::Audio::AudioFormat& format) {
    using namespace Engine::Audio;
    VoiceProcessorConfig config;
    config.format = format;
    config.pitchShiftWindowFrames = 256;
    config.syntheticVoice = {true, 0.6f, 0.75f, 0.55f, 0.35f, 0.8f};
    VoiceProcessor voice;
    if (!Check(static_cast<bool>(voice.Prepare(config)), "synthetic voice prepare")) return false;
    AudioBuffer buffer(format.channelCount, format.framesPerBuffer);
    for (int block = 0; block < 30; ++block) {
        FillTone(buffer, static_cast<std::size_t>(block) * buffer.FrameCount(), format.sampleRate);
        AudioProcessContext context{buffer.View(), buffer.View(), format.sampleRate,
            static_cast<std::uint64_t>(block) * buffer.FrameCount()};
        voice.Process(context);
        if (!Check(Finite(buffer), "synthetic voice chain finite")) return false;
    }
    const auto metrics = voice.CaptureMetrics();
    if (!Check(metrics.state.syntheticVoiceEnabled && metrics.syntheticVoice.snapshotRevision == 1,
               "synthetic voice state")) return false;
    for (std::uint64_t revision = 2; revision <= 1001; ++revision) {
        config.syntheticVoice.amount = static_cast<float>(revision % 100) / 99.0f;
        if (!Check(static_cast<bool>(voice.PublishSyntheticVoice(
            BuildSyntheticVoiceSnapshot(config.syntheticVoice, revision))),
                   "synthetic snapshot stress")) return false;
    }
    voice.Reset();
    return Check(voice.CaptureMetrics().processedFrames == 0, "voice reset");
}

} // namespace

int main() {
    using namespace Engine::Audio;
    if (!TestStabilizer()) return EXIT_FAILURE;
    for (const AudioFormat format : {
        AudioFormat{44'100, 1, AudioSampleFormat::Float32, 128},
        AudioFormat{48'000, 2, AudioSampleFormat::Float32, 128}}) {
        if (!TestMicroPitch(format) || !TestVoiceIntegration(format)) return EXIT_FAILURE;
    }
    std::cout << "Synthetic Voice: stabilizer confidence gate, micro pitch mono/stereo "
                 "44.1/48 kHz, snapshot stress, VoiceProcessor integration passed\n";
    return EXIT_SUCCESS;
}
