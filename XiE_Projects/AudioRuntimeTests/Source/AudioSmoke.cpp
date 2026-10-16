#include "Audio/Backend/WASAPI/WasapiBackend.h"
#include "Audio/Core/AudioSpscRingBuffer.h"
#include "Audio/Diagnostics/AudioDiagnostics.h"
#include "Audio/DSP/Noise/NoiseFrameAdapter.h"
#include "Audio/DSP/Noise/NoiseSuppressorProcessor.h"
#include "Audio/DSP/Noise/RNNoiseSuppressor.h"
#include "Audio/Voice/VoiceProcessor.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace {

using namespace Engine::Audio;

void PrintWide(std::wstring_view text) {
    DWORD consoleMode = 0;
    const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (output != INVALID_HANDLE_VALUE && GetConsoleMode(output, &consoleMode)) {
        DWORD written = 0;
        WriteConsoleW(output, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
        return;
    }
    const int size = WideCharToMultiByte(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string utf8(static_cast<std::size_t>(std::max(0, size)), '\0');
    if (size > 0) {
        WideCharToMultiByte(
            CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
            utf8.data(), size, nullptr, nullptr);
        std::cout << utf8;
    }
}

struct SmokeRuntime {
    AudioSpscRingBuffer ring;
    VoiceProcessor voice;
    AudioBuffer processBuffer;
    AudioDiagnostics diagnostics;
    bool useVoice = false;
    std::uint32_t sampleRate = 48'000;
    std::atomic<bool> outputPrimed{false};

    static void Capture(
        void* user,
        ConstAudioBufferView input,
        std::uint64_t streamFrame) noexcept {
        auto& runtime = *static_cast<SmokeRuntime*>(user);
        const auto begin = std::chrono::steady_clock::now();
        ConstAudioBufferView source = input;
        if (runtime.useVoice) {
            AudioBufferView full = runtime.processBuffer.View();
            AudioBufferView block(
                full.Data(), full.ChannelCount(), input.FrameCount(), full.ChannelStride());
            for (std::size_t channel = 0; channel < block.ChannelCount(); ++channel) {
                const std::size_t inputChannel = std::min(channel, input.ChannelCount() - 1);
                std::copy_n(input.Channel(inputChannel).data(), input.FrameCount(), block.Channel(channel).data());
            }
            AudioProcessContext context{block, block, runtime.sampleRate, streamFrame};
            runtime.voice.Process(context);
            source = ConstAudioBufferView(
                block.Data(), block.ChannelCount(), block.FrameCount(), block.ChannelStride());
        }
        const std::size_t pushed = runtime.ring.Push(source);
        if (pushed != source.FrameCount()) {
            runtime.diagnostics.RecordXRun(source.FrameCount() - pushed);
        }
        const auto end = std::chrono::steady_clock::now();
        runtime.diagnostics.RecordCallback(
            std::chrono::duration<double, std::milli>(end - begin).count());
    }

    static void Render(
        void* user,
        AudioBufferView output,
        std::uint64_t) noexcept {
        auto& runtime = *static_cast<SmokeRuntime*>(user);
        if (!runtime.outputPrimed.load(std::memory_order_acquire)) {
            if (runtime.ring.AvailableFrames() < output.FrameCount()) {
                output.Clear();
                return;
            }
            runtime.outputPrimed.store(true, std::memory_order_release);
        }
        const std::size_t popped = runtime.ring.Pop(output);
        if (popped != output.FrameCount()) {
            runtime.diagnostics.RecordXRun(output.FrameCount() - popped);
        }
    }
};

int SelfTest() {
    AudioSpscRingBuffer ring;
    ring.Prepare(2, 16);
    AudioBuffer input(2, 8);
    for (std::size_t frame = 0; frame < 8; ++frame) {
        input.View().Sample(0, frame) = static_cast<float>(frame);
        input.View().Sample(1, frame) = static_cast<float>(100 + frame);
    }
    const AudioBuffer& constant = input;
    AudioBuffer output(2, 8);
    bool passed = ring.Push(constant.View()) == 8 &&
        ring.Pop(output.View()) == 8 &&
        output.View().Sample(0, 7) == 7.0f &&
        output.View().Sample(1, 7) == 107.0f;

    const AudioFormat format{48'000, 1, AudioSampleFormat::Float32, 128};
    RNNoiseSuppressor algorithm;
    passed = passed && algorithm.Prepare({format}) && algorithm.RequiredFrameSize() > 0;
    NoiseFrameAdapter adapter;
    passed = passed && adapter.Prepare(algorithm.RequiredFrameSize());
    std::vector<float> mono(64, 0.0f);
    std::vector<float> dry(64, 0.0f);
    std::vector<float> processed(64, 0.0f);
    for (int block = 0; passed && block < 12; ++block) {
        passed = adapter.Process(mono, dry, processed, algorithm).valid;
    }
    NoiseSuppressorProcessor processor;
    passed = passed && processor.Prepare(format);
    NoiseSuppressorConfig noise;
    noise.enabled = true;
    passed = passed && processor.Publish(BuildNoiseSuppressorSnapshot(noise, 1));
    AudioBuffer noiseBuffer(1, format.framesPerBuffer);
    AudioProcessContext noiseContext{
        noiseBuffer.View(), noiseBuffer.View(), format.sampleRate, 0};
    for (int block = 0; passed && block < 8; ++block) processor.Process(noiseContext);
    passed = passed && processor.CaptureMetrics().active;
    std::cout << (passed ? "XiEAudioSmoke self-test passed\n" : "XiEAudioSmoke self-test failed\n");
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}

void PrintUsage() {
    std::wcout
        << L"XiEAudioSmoke --list\n"
        << L"XiEAudioSmoke --run --input N --output N [--seconds N] [--voice]\n"
        << L"XiEAudioSmoke --run --input N --output N --voice --interactive\n";
}

std::shared_ptr<const Engine::Curve::CurveSnapshot> ParseCurve(
    std::wistringstream& input,
    float minimum,
    float maximum) {
    Engine::Curve::CurveModel model;
    float x = 0;
    float y = 0;
    Engine::Curve::CurvePointId id = 1;
    while (input >> x >> y) {
        if (model.AddPoint({id++, x, y, Engine::Curve::CurveInterpolation::Smooth}) !=
            Engine::Curve::CurveStatus::Ok) {
            return {};
        }
    }
    if (model.Points().empty()) return {};
    return Engine::Curve::BuildCurveSnapshot(model, 1024, minimum, maximum);
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    bool list = false;
    bool run = false;
    bool voice = false;
    bool interactive = false;
    bool selfTest = false;
    int inputIndex = -1;
    int outputIndex = -1;
    int seconds = 10;
    for (int i = 1; i < argc; ++i) {
        const std::wstring argument = argv[i];
        if (argument == L"--list") list = true;
        else if (argument == L"--run") run = true;
        else if (argument == L"--voice") voice = true;
        else if (argument == L"--interactive") interactive = true;
        else if (argument == L"--self-test") selfTest = true;
        else if (argument == L"--input" && i + 1 < argc) inputIndex = std::stoi(argv[++i]);
        else if (argument == L"--output" && i + 1 < argc) outputIndex = std::stoi(argv[++i]);
        else if (argument == L"--seconds" && i + 1 < argc) seconds = std::max(1, std::stoi(argv[++i]));
        else {
            PrintUsage();
            return EXIT_FAILURE;
        }
    }
    if (selfTest) {
        return SelfTest();
    }

    WasapiBackend backend;
    AudioStatus status;
    const auto inputs = backend.EnumerateDevices(AudioDeviceDirection::Input, &status);
    if (!status) {
        std::wcerr << L"WASAPI input enumeration failed: " << status.platformCode << L'\n';
        return EXIT_FAILURE;
    }
    const auto outputs = backend.EnumerateDevices(AudioDeviceDirection::Output, &status);
    if (!status) {
        std::wcerr << L"WASAPI output enumeration failed: " << status.platformCode << L'\n';
        return EXIT_FAILURE;
    }
    PrintWide(L"Inputs:\n" + DumpAudioDevices(inputs) +
        L"Outputs:\n" + DumpAudioDevices(outputs));
    if (list || !run) {
        if (!list) PrintUsage();
        return EXIT_SUCCESS;
    }
    if (inputIndex < 0 || outputIndex < 0 ||
        inputIndex >= static_cast<int>(inputs.size()) ||
        outputIndex >= static_cast<int>(outputs.size())) {
        std::wcerr << L"Select valid --input and --output indices.\n";
        return EXIT_FAILURE;
    }

    AudioStreamConfig inputConfig;
    inputConfig.deviceId = inputs[inputIndex].id;
    inputConfig.requestedFormat = {48'000, 1, AudioSampleFormat::Float32, 128};
    AudioStreamConfig outputConfig = inputConfig;
    outputConfig.deviceId = outputs[outputIndex].id;
    auto input = backend.OpenInput(inputConfig, &status);
    if (!input) {
        std::wcerr << L"Input open failed: " << status.platformCode << L'\n';
        return EXIT_FAILURE;
    }
    auto output = backend.OpenOutput(outputConfig, &status);
    if (!output) {
        std::wcerr << L"Output open failed: " << status.platformCode << L'\n';
        return EXIT_FAILURE;
    }

    SmokeRuntime runtime;
    runtime.useVoice = voice;
    runtime.sampleRate = input->Format().sampleRate;
    runtime.ring.Prepare(1, 48'000);
    runtime.processBuffer.Resize(1, input->Format().framesPerBuffer);
    VoiceProcessorConfig voiceConfig;
    voiceConfig.format = input->Format();
    if (!runtime.voice.Prepare(voiceConfig)) {
        std::wcerr << L"Voice processor prepare failed.\n";
        return EXIT_FAILURE;
    }
    NoiseSuppressorConfig noiseConfig;
    std::uint64_t noiseRevision = 1;
    auto publishNoise = [&]() {
        return runtime.voice.PublishNoiseSuppression(
            BuildNoiseSuppressorSnapshot(noiseConfig, noiseRevision++));
    };
    AudioLatency latency = input->Latency();
    latency.outputMilliseconds = output->Latency().outputMilliseconds;
    latency.processingMilliseconds =
        1000.0 * runtime.voice.LatencyFrames() / input->Format().sampleRate;
    runtime.diagnostics.SetLatency(latency);

    bool running = false;
    auto startStreams = [&]() {
        if (running) return true;
        runtime.ring.Reset();
        runtime.outputPrimed.store(false, std::memory_order_release);
        status = output->Start(&SmokeRuntime::Render, &runtime);
        if (!status) {
            std::wcerr << L"Output start failed: " << status.platformCode << L'\n';
            return false;
        }
        status = input->Start(&SmokeRuntime::Capture, &runtime);
        if (!status) {
            (void)output->Stop();
            std::wcerr << L"Input start failed: " << status.platformCode << L'\n';
            return false;
        }
        running = true;
        return true;
    };
    auto stopStreams = [&]() {
        if (!running) return;
        (void)input->Stop();
        (void)output->Stop();
        running = false;
    };
    if (!startStreams()) return EXIT_FAILURE;
    std::wcout << L"Running " << (voice ? L"voice chain" : L"passthrough")
               << (interactive ? L" in interactive mode.\n" : L" for ")
               << (interactive ? L"" : std::to_wstring(seconds) + L" seconds...\n");
    if (interactive) {
        std::wcout
            << L"Commands: start, stop, restart, bypass 0|1, wet V, ingain V, "
               L"outgain V, pitch V, formant V,\n"
               L"noise on|off, noise bypass on|off, noise strength V, noise wet V, "
               L"noise reset, noise status,\n"
               L"pitchcurve X Y [X Y...], formantcurve X Y [X Y...], metrics, quit\n";
        std::wstring line;
        while (std::getline(std::wcin, line)) {
            std::wistringstream command(line);
            std::wstring name;
            command >> name;
            if (name == L"quit") break;
            if (name == L"start") {
                std::wcout << (startStreams() ? L"Started\n" : L"Start failed\n");
            } else if (name == L"stop") {
                stopStreams();
                std::wcout << L"Stopped; endpoints remain open for restart\n";
            } else if (name == L"restart") {
                stopStreams();
                input->Reset();
                output->Reset();
                runtime.voice.Reset();
                std::wcout << (startStreams() ? L"Restarted\n" : L"Restart failed\n");
            } else if (name == L"bypass" || name == L"wet" || name == L"ingain" ||
                name == L"outgain" || name == L"pitch" || name == L"formant") {
                float value = 0;
                if (!(command >> value)) {
                    std::wcout << L"Missing numeric value\n";
                    continue;
                }
                const AudioParameterId id =
                    name == L"bypass" ? AudioParameterId::Bypass :
                    name == L"wet" ? AudioParameterId::Wet :
                    name == L"ingain" ? AudioParameterId::Gain :
                    name == L"outgain" ? AudioParameterId::OutputGain :
                    name == L"pitch" ? AudioParameterId::PitchSemitones :
                    AudioParameterId::FormantSemitones;
                runtime.voice.SetParameter(id, value);
                std::wcout << L"Updated without stream restart\n";
            } else if (name == L"pitchcurve" || name == L"formantcurve") {
                auto snapshot = ParseCurve(command, 70.0f, 500.0f);
                if (!snapshot) {
                    std::wcout << L"Invalid curve; provide X Y pairs\n";
                    continue;
                }
                VoiceCurveSet curves;
                if (name == L"pitchcurve") curves.pitch = std::move(snapshot);
                else curves.formant = std::move(snapshot);
                const AudioStatus publish = runtime.voice.PublishCurves(curves);
                std::wcout << (publish ? L"Curve snapshot published\n" : L"Curve publish failed\n");
            } else if (name == L"noise") {
                std::wstring action;
                command >> action;
                if (action == L"on" || action == L"off") {
                    noiseConfig.enabled = action == L"on";
                    std::wcout << (publishNoise() ? L"Noise snapshot published\n" :
                        L"Noise publish failed\n");
                } else if (action == L"bypass") {
                    std::wstring value;
                    command >> value;
                    if (value != L"on" && value != L"off") {
                        std::wcout << L"Use: noise bypass on|off\n";
                        continue;
                    }
                    runtime.voice.SetNoiseBypassed(value == L"on");
                    std::wcout << L"Noise bypass updated\n";
                } else if (action == L"strength" || action == L"wet") {
                    float value = 0.0f;
                    if (!(command >> value)) {
                        std::wcout << L"Missing numeric value\n";
                        continue;
                    }
                    value = std::clamp(value, 0.0f, 1.0f);
                    if (action == L"strength") noiseConfig.strength = value;
                    else noiseConfig.wet = value;
                    std::wcout << (publishNoise() ? L"Noise snapshot published\n" :
                        L"Noise publish failed\n");
                } else if (action == L"reset") {
                    noiseConfig = {};
                    runtime.voice.SetNoiseBypassed(false);
                    std::wcout << (publishNoise() ? L"Noise reset\n" : L"Noise reset failed\n");
                } else if (action == L"status") {
                    std::cout << DumpVoiceState(runtime.voice.CaptureMetrics());
                } else {
                    std::wcout << L"Unknown noise command\n";
                }
            } else if (name == L"metrics") {
                std::cout << DumpAudioState(input->Format(), runtime.diagnostics.Capture())
                          << DumpVoiceState(runtime.voice.CaptureMetrics());
            } else if (!name.empty()) {
                std::wcout << L"Unknown command\n";
            }
        }
    } else {
        std::this_thread::sleep_for(std::chrono::seconds(seconds));
    }
    stopStreams();

    const auto metrics = runtime.diagnostics.Capture();
    std::cout << DumpAudioState(input->Format(), metrics);
    const bool healthy = input->State() != AudioStreamState::DeviceLost &&
        output->State() != AudioStreamState::DeviceLost;
    return healthy ? EXIT_SUCCESS : EXIT_FAILURE;
}
