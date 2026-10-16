#include "Audio/DSP/Delay/DelayLine.h"
#include "Audio/DSP/Diffusion/AllPassDiffuser.h"
#include "Audio/DSP/Haze/SoftHaze.h"
#include "Audio/DSP/Haze/SpatialHaze.h"
#include "Audio/Voice/VoiceProcessor.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

bool Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

bool Finite(const Engine::Audio::AudioBuffer& buffer) {
    for (std::size_t channel = 0; channel < buffer.ChannelCount(); ++channel) {
        for (const float sample : buffer.View().Channel(channel)) {
            if (!std::isfinite(sample)) return false;
        }
    }
    return true;
}

void FillTone(Engine::Audio::AudioBuffer& buffer, std::size_t offset = 0,
    std::uint32_t sampleRate = 48'000) {
    for (std::size_t frame = 0; frame < buffer.FrameCount(); ++frame) {
        const float sample = 0.2f * std::sin(
            2.0 * 3.14159265358979323846 * 440.0 * static_cast<double>(frame + offset) /
            static_cast<double>(sampleRate));
        for (std::size_t channel = 0; channel < buffer.ChannelCount(); ++channel) {
            buffer.View().Sample(channel, frame) = sample;
        }
    }
}

bool TestDelayAndDiffusion(const Engine::Audio::AudioFormat& format) {
    using namespace Engine::Audio;
    DelayLine delay;
    if (!Check(static_cast<bool>(delay.Prepare(format, 8)), "DelayLine prepare")) return false;
    delay.Write(0, 1.0f);
    if (format.channelCount >= 2) delay.Write(1, -1.0f);
    delay.Advance();
    if (!Check(std::abs(delay.Read(0, 1) - 1.0f) < 1.0e-6f, "DelayLine mono/stereo wrap")) return false;
    if (format.channelCount >= 2 &&
        !Check(std::abs(delay.Read(1, 1) + 1.0f) < 1.0e-6f, "DelayLine stereo channel")) return false;
    delay.Reset();
    if (!Check(std::abs(delay.Read(0, 1)) < 1.0e-6f, "DelayLine reset")) return false;

    AllPassDiffuser diffuser;
    if (!Check(static_cast<bool>(diffuser.Prepare(format, 64)), "AllPass prepare")) return false;
    diffuser.SetParameters(7, 0.45f, 0.35f);
    for (int index = 0; index < 512; ++index) {
        const float input = index == 0 ? 1.0f : 0.0f;
        const float output = diffuser.ProcessChannel(0, input);
        if (!Check(std::isfinite(output) && std::abs(output) < 8.0f,
                   "AllPass finite and stable")) return false;
        diffuser.Advance();
    }
    return true;
}

bool TestSoftHaze(const Engine::Audio::AudioFormat& format) {
    using namespace Engine::Audio;
    SoftHazeProcessor processor;
    if (!Check(static_cast<bool>(processor.Prepare(format)), "SoftHaze prepare")) return false;

    AudioBuffer dry(format.channelCount, format.framesPerBuffer);
    FillTone(dry, 0, format.sampleRate);
    AudioBuffer original = dry;
    AudioProcessContext context{dry.View(), dry.View(), format.sampleRate, 0};
    processor.Process(context);
    for (std::size_t channel = 0; channel < dry.ChannelCount(); ++channel) {
        for (std::size_t frame = 0; frame < dry.FrameCount(); ++frame) {
            if (!Check(std::abs(dry.View().Sample(channel, frame) -
                original.View().Sample(channel, frame)) < 1.0e-6f,
                "SoftHaze disabled is dry")) return false;
        }
    }

    SoftHazeConfig config;
    config.enabled = true;
    config.amount = 0.85f;
    config.blur = 0.55f;
    config.damping = 0.50f;
    config.wet = 0.55f;
    if (!Check(static_cast<bool>(processor.Publish(BuildSoftHazeSnapshot(config, 1))),
               "SoftHaze publish")) return false;
    bool changed = false;
    for (int block = 0; block < 80; ++block) {
        FillTone(dry, static_cast<std::size_t>(block) * dry.FrameCount(), format.sampleRate);
        original = dry;
        context = {dry.View(), dry.View(), format.sampleRate,
            static_cast<std::uint64_t>(block) * dry.FrameCount()};
        processor.Process(context);
        if (!Finite(dry)) return false;
        for (std::size_t channel = 0; channel < dry.ChannelCount(); ++channel) {
            for (std::size_t frame = 0; frame < dry.FrameCount(); ++frame) {
                changed = changed || std::abs(dry.View().Sample(channel, frame) -
                    original.View().Sample(channel, frame)) > 1.0e-5f;
            }
        }
    }
    const SoftHazeMetrics metrics = processor.CaptureMetrics();
    if (!Check(metrics.enabled && metrics.snapshotRevision == 1, "SoftHaze metrics")) return false;
    if (!Check(metrics.effectiveDelaySamples > 0 && metrics.delayMemoryBytes > 0,
               "SoftHaze delay metrics")) return false;
    if (!Check(changed, "SoftHaze changes signal when enabled")) return false;
    for (std::uint64_t revision = 2; revision <= 1001; ++revision) {
        config.blur = static_cast<float>(revision % 100) / 99.0f;
        if (!Check(static_cast<bool>(processor.Publish(BuildSoftHazeSnapshot(config, revision))),
                   "SoftHaze snapshot stress")) return false;
    }
    processor.Reset();
    return Check(processor.CaptureMetrics().processedFrames == 0, "SoftHaze reset");
}

bool TestSpatialHaze(const Engine::Audio::AudioFormat& format) {
    using namespace Engine::Audio;
    SpatialHazeProcessor processor;
    if (!Check(static_cast<bool>(processor.Prepare(format)), "SpatialHaze prepare")) return false;

    AudioBuffer dry(format.channelCount, format.framesPerBuffer);
    FillTone(dry, 0, format.sampleRate);
    AudioBuffer original = dry;
    AudioProcessContext context{dry.View(), dry.View(), format.sampleRate, 0};
    processor.Process(context);
    for (std::size_t channel = 0; channel < dry.ChannelCount(); ++channel) {
        for (std::size_t frame = 0; frame < dry.FrameCount(); ++frame) {
            if (!Check(std::abs(dry.View().Sample(channel, frame) -
                original.View().Sample(channel, frame)) < 1.0e-6f,
                "SpatialHaze disabled is dry")) return false;
        }
    }

    SpatialHazeConfig config;
    config.enabled = true;
    config.amount = 0.80f;
    config.size = 0.55f;
    config.diffusion = 0.75f;
    config.damping = 0.45f;
    config.width = 0.85f;
    config.wet = 0.40f;
    if (!Check(static_cast<bool>(processor.Publish(BuildSpatialHazeSnapshot(config, 1))),
               "SpatialHaze publish")) return false;
    bool changed = false;
    for (int block = 0; block < 100; ++block) {
        FillTone(dry, static_cast<std::size_t>(block) * dry.FrameCount(), format.sampleRate);
        original = dry;
        context = {dry.View(), dry.View(), format.sampleRate,
            static_cast<std::uint64_t>(block) * dry.FrameCount()};
        processor.Process(context);
        if (!Finite(dry)) return false;
        for (std::size_t channel = 0; channel < dry.ChannelCount(); ++channel) {
            for (std::size_t frame = 0; frame < dry.FrameCount(); ++frame) {
                changed = changed || std::abs(dry.View().Sample(channel, frame) -
                    original.View().Sample(channel, frame)) > 1.0e-5f;
            }
        }
    }
    const SpatialHazeMetrics metrics = processor.CaptureMetrics();
    if (!Check(metrics.enabled && metrics.snapshotRevision == 1, "SpatialHaze metrics")) return false;
    if (!Check(metrics.effectiveTailMilliseconds > 0.0 && metrics.delayMemoryBytes > 0,
               "SpatialHaze tail metrics")) return false;
    if (!Check(changed, "SpatialHaze changes signal when enabled")) return false;

    SpatialHazeConfig monoConfig = config;
    monoConfig.width = 0.0f;
    if (!Check(static_cast<bool>(processor.Publish(BuildSpatialHazeSnapshot(monoConfig, 2))),
               "SpatialHaze width publish")) return false;
    for (int block = 0; block < 20; ++block) {
        FillTone(dry, static_cast<std::size_t>(block + 100) * dry.FrameCount(), format.sampleRate);
        context = {dry.View(), dry.View(), format.sampleRate,
            static_cast<std::uint64_t>(block + 100) * dry.FrameCount()};
        processor.Process(context);
        if (format.channelCount >= 2) {
            for (std::size_t frame = 0; frame < dry.FrameCount(); ++frame) {
                if (!Check(std::abs(dry.View().Sample(0, frame) - dry.View().Sample(1, frame)) < 1.0e-5f,
                           "SpatialHaze width zero mono compatibility")) return false;
            }
        }
    }
    for (std::uint64_t revision = 3; revision <= 1002; ++revision) {
        config.size = static_cast<float>(revision % 100) / 99.0f;
        if (!Check(static_cast<bool>(processor.Publish(BuildSpatialHazeSnapshot(config, revision))),
                   "SpatialHaze snapshot stress")) return false;
    }
    processor.Reset();
    return Check(processor.CaptureMetrics().processedFrames == 0, "SpatialHaze reset");
}

bool TestVoiceChain(const Engine::Audio::AudioFormat& format) {
    using namespace Engine::Audio;
    VoiceProcessorConfig config;
    config.format = format;
    config.pitchShiftWindowFrames = 256;
    config.softHaze.enabled = true;
    config.spatialHaze.enabled = true;
    VoiceProcessor processor;
    if (!Check(static_cast<bool>(processor.Prepare(config)), "VoiceProcessor haze prepare")) return false;
    AudioBuffer buffer(format.channelCount, format.framesPerBuffer);
    for (int block = 0; block < 24; ++block) {
        FillTone(buffer, static_cast<std::size_t>(block) * buffer.FrameCount(), format.sampleRate);
        AudioProcessContext context{buffer.View(), buffer.View(), format.sampleRate,
            static_cast<std::uint64_t>(block) * buffer.FrameCount()};
        processor.Process(context);
        if (!Check(Finite(buffer), "VoiceProcessor haze finite")) return false;
    }
    const VoiceProcessorMetrics metrics = processor.CaptureMetrics();
    return Check(metrics.state.softHazeEnabled && metrics.state.spatialHazeEnabled,
                 "VoiceProcessor haze state");
}

} // namespace

int main() {
    using namespace Engine::Audio;
    for (const AudioFormat format : {
        AudioFormat{44'100, 1, AudioSampleFormat::Float32, 128},
        AudioFormat{48'000, 2, AudioSampleFormat::Float32, 128}}) {
        if (!format.IsValid()) return EXIT_FAILURE;
        if (!TestDelayAndDiffusion(format)) return EXIT_FAILURE;
        if (!TestSoftHaze(format)) return EXIT_FAILURE;
        if (!TestSpatialHaze(format)) return EXIT_FAILURE;
        if (!TestVoiceChain(format)) return EXIT_FAILURE;
    }
    std::cout << "Haze DSP: delay/diffusion, Soft Haze, Spatial Haze, mono/stereo 44.1/48 kHz, "
                 "snapshot stress, voice-chain integration passed\n";
    return EXIT_SUCCESS;
}
