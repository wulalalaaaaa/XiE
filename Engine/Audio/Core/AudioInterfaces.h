#pragma once

#include "Audio/Core/AudioBuffer.h"
#include "Audio/Core/AudioTypes.h"

#include <memory>
#include <span>
#include <vector>

namespace Engine::Audio {

using AudioInputCallback = void(*)(
    void* userData,
    ConstAudioBufferView input,
    std::uint64_t streamFrame) noexcept;

using AudioOutputCallback = void(*)(
    void* userData,
    AudioBufferView output,
    std::uint64_t streamFrame) noexcept;

class IAudioInputStream {
public:
    virtual ~IAudioInputStream() = default;
    virtual AudioStatus Start(AudioInputCallback callback, void* userData) = 0;
    virtual AudioStatus Stop() = 0;
    virtual void Reset() noexcept = 0;
    [[nodiscard]] virtual AudioStreamState State() const noexcept = 0;
    [[nodiscard]] virtual AudioFormat Format() const noexcept = 0;
    [[nodiscard]] virtual AudioLatency Latency() const noexcept = 0;
};

class IAudioOutputStream {
public:
    virtual ~IAudioOutputStream() = default;
    virtual AudioStatus Start(AudioOutputCallback callback, void* userData) = 0;
    virtual AudioStatus Stop() = 0;
    virtual void Reset() noexcept = 0;
    [[nodiscard]] virtual AudioStreamState State() const noexcept = 0;
    [[nodiscard]] virtual AudioFormat Format() const noexcept = 0;
    [[nodiscard]] virtual AudioLatency Latency() const noexcept = 0;
};

class IAudioBackend {
public:
    virtual ~IAudioBackend() = default;
    [[nodiscard]] virtual std::vector<AudioDeviceInfo> EnumerateDevices(
        AudioDeviceDirection direction,
        AudioStatus* status = nullptr) = 0;
    [[nodiscard]] virtual std::unique_ptr<IAudioInputStream> OpenInput(
        const AudioStreamConfig& config,
        AudioStatus* status = nullptr) = 0;
    [[nodiscard]] virtual std::unique_ptr<IAudioOutputStream> OpenOutput(
        const AudioStreamConfig& config,
        AudioStatus* status = nullptr) = 0;
};

} // namespace Engine::Audio
