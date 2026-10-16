#pragma once

#include "Audio/Core/AudioInterfaces.h"

namespace Engine::Audio {

class WasapiBackend final : public IAudioBackend {
public:
    WasapiBackend() noexcept;
    ~WasapiBackend() override;

    WasapiBackend(const WasapiBackend&) = delete;
    WasapiBackend& operator=(const WasapiBackend&) = delete;
    WasapiBackend(WasapiBackend&&) = delete;
    WasapiBackend& operator=(WasapiBackend&&) = delete;

    [[nodiscard]] std::vector<AudioDeviceInfo> EnumerateDevices(
        AudioDeviceDirection direction,
        AudioStatus* status = nullptr) override;
    [[nodiscard]] std::unique_ptr<IAudioInputStream> OpenInput(
        const AudioStreamConfig& config,
        AudioStatus* status = nullptr) override;
    [[nodiscard]] std::unique_ptr<IAudioOutputStream> OpenOutput(
        const AudioStreamConfig& config,
        AudioStatus* status = nullptr) override;

private:
    bool m_ComInitialized = false;
};

} // namespace Engine::Audio
