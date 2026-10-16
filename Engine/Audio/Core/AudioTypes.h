#pragma once

#include <cstdint>
#include <string>

namespace Engine::Audio {

using AudioDeviceId = std::wstring;

enum class AudioDeviceDirection : std::uint8_t {
    Input,
    Output
};

enum class AudioSampleFormat : std::uint8_t {
    Float32,
    Int16,
    Int24,
    Int32,
    Unknown
};

struct AudioFormat {
    std::uint32_t sampleRate = 48'000;
    std::uint16_t channelCount = 1;
    AudioSampleFormat sampleFormat = AudioSampleFormat::Float32;
    std::uint32_t framesPerBuffer = 128;

    [[nodiscard]] bool IsValid() const noexcept {
        return sampleRate > 0 && channelCount > 0 && framesPerBuffer > 0 &&
            sampleFormat != AudioSampleFormat::Unknown;
    }
};

struct AudioDeviceInfo {
    AudioDeviceId id;
    std::wstring name;
    AudioDeviceDirection direction = AudioDeviceDirection::Input;
    bool isDefault = false;
    AudioFormat mixFormat{};
};

enum class AudioShareMode : std::uint8_t {
    Shared,
    Exclusive
};

struct AudioStreamConfig {
    AudioDeviceId deviceId;
    AudioFormat requestedFormat{};
    AudioShareMode shareMode = AudioShareMode::Shared;
    bool lowLatency = true;
};

enum class AudioStreamState : std::uint8_t {
    Stopped,
    Starting,
    Running,
    DeviceLost,
    Recovering,
    Error
};

enum class AudioStatusCode : std::uint16_t {
    Ok,
    InvalidArgument,
    UnsupportedFormat,
    UnsupportedMode,
    BackendUnavailable,
    DeviceNotFound,
    DeviceInUse,
    DeviceLost,
    InitializationFailed,
    StartFailed,
    StopFailed,
    PlatformError
};

struct AudioStatus {
    AudioStatusCode code = AudioStatusCode::Ok;
    std::int64_t platformCode = 0;

    [[nodiscard]] explicit operator bool() const noexcept {
        return code == AudioStatusCode::Ok;
    }
};

struct AudioLatency {
    double captureMilliseconds = 0.0;
    double processingMilliseconds = 0.0;
    double outputMilliseconds = 0.0;

    [[nodiscard]] double TotalEstimatedMilliseconds() const noexcept {
        return captureMilliseconds + processingMilliseconds + outputMilliseconds;
    }
};

} // namespace Engine::Audio
