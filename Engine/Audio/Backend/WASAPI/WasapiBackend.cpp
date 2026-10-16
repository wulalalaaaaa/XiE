#include "Audio/Backend/WASAPI/WasapiBackend.h"

#include <windows.h>
#include <propsys.h>
#include <propkeydef.h>
#include <audioclient.h>
#include <avrt.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ksmedia.h>
#include <propvarutil.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>
#include <utility>

namespace Engine::Audio {
namespace {

using Microsoft::WRL::ComPtr;

AudioStatus StatusFromHResult(HRESULT result, AudioStatusCode fallback) noexcept {
    AudioStatusCode code = fallback;
    if (result == AUDCLNT_E_DEVICE_INVALIDATED ||
        result == AUDCLNT_E_RESOURCES_INVALIDATED ||
        result == AUDCLNT_E_SERVICE_NOT_RUNNING) {
        code = AudioStatusCode::DeviceLost;
    } else if (result == AUDCLNT_E_DEVICE_IN_USE || result == AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED) {
        code = AudioStatusCode::DeviceInUse;
    } else if (result == AUDCLNT_E_UNSUPPORTED_FORMAT) {
        code = AudioStatusCode::UnsupportedFormat;
    }
    return {code, static_cast<std::int64_t>(result)};
}

class ScopedCom {
public:
    ScopedCom() noexcept {
        m_Result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    }
    ~ScopedCom() {
        if (SUCCEEDED(m_Result)) {
            CoUninitialize();
        }
    }
    [[nodiscard]] bool Available() const noexcept {
        return SUCCEEDED(m_Result) || m_Result == RPC_E_CHANGED_MODE;
    }
    [[nodiscard]] HRESULT Result() const noexcept { return m_Result; }

private:
    HRESULT m_Result = E_FAIL;
};

EDataFlow Flow(AudioDeviceDirection direction) noexcept {
    return direction == AudioDeviceDirection::Input ? eCapture : eRender;
}

AudioSampleFormat SampleFormat(const WAVEFORMATEX& format) noexcept {
    if (format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT && format.wBitsPerSample == 32) {
        return AudioSampleFormat::Float32;
    }
    if (format.wFormatTag == WAVE_FORMAT_PCM) {
        if (format.wBitsPerSample == 16) {
            return AudioSampleFormat::Int16;
        }
        if (format.wBitsPerSample == 24) {
            return AudioSampleFormat::Int24;
        }
        if (format.wBitsPerSample == 32) {
            return AudioSampleFormat::Int32;
        }
    }
    if (format.wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        const auto& extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE&>(format);
        if (extensible.SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT &&
            extensible.Samples.wValidBitsPerSample == 32) {
            return AudioSampleFormat::Float32;
        }
        if (extensible.SubFormat == KSDATAFORMAT_SUBTYPE_PCM) {
            if (format.wBitsPerSample == 16) {
                return AudioSampleFormat::Int16;
            }
            if (format.wBitsPerSample == 24) {
                return AudioSampleFormat::Int24;
            }
            if (format.wBitsPerSample == 32) {
                return AudioSampleFormat::Int32;
            }
        }
    }
    return AudioSampleFormat::Unknown;
}

AudioFormat ToAudioFormat(const WAVEFORMATEX& format, std::uint32_t frames) noexcept {
    return {
        format.nSamplesPerSec,
        format.nChannels,
        SampleFormat(format),
        frames
    };
}

WAVEFORMATEX FloatWaveFormat(const AudioFormat& format) noexcept {
    WAVEFORMATEX result{};
    result.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    result.nChannels = format.channelCount;
    result.nSamplesPerSec = format.sampleRate;
    result.wBitsPerSample = 32;
    result.nBlockAlign = static_cast<WORD>(result.nChannels * sizeof(float));
    result.nAvgBytesPerSec = result.nSamplesPerSec * result.nBlockAlign;
    return result;
}

AudioStatus CreateEnumerator(ComPtr<IMMDeviceEnumerator>& enumerator) {
    const HRESULT result = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        IID_PPV_ARGS(&enumerator));
    return SUCCEEDED(result)
        ? AudioStatus{}
        : StatusFromHResult(result, AudioStatusCode::BackendUnavailable);
}

AudioStatus ResolveDevice(
    AudioDeviceDirection direction,
    const AudioDeviceId& id,
    ComPtr<IMMDevice>& device) {
    ComPtr<IMMDeviceEnumerator> enumerator;
    AudioStatus status = CreateEnumerator(enumerator);
    if (!status) {
        return status;
    }
    HRESULT result = id.empty()
        ? enumerator->GetDefaultAudioEndpoint(Flow(direction), eConsole, &device)
        : enumerator->GetDevice(id.c_str(), &device);
    return SUCCEEDED(result)
        ? AudioStatus{}
        : StatusFromHResult(result, AudioStatusCode::DeviceNotFound);
}

struct PreparedClient {
    ComPtr<IAudioClient> client;
    AudioFormat format;
    UINT32 endpointFrames = 0;
    AudioLatency latency{};
};

AudioStatus PrepareClient(
    AudioDeviceDirection direction,
    const AudioStreamConfig& config,
    PreparedClient& prepared) {
    if (!config.requestedFormat.IsValid() ||
        config.requestedFormat.sampleFormat != AudioSampleFormat::Float32 ||
        config.requestedFormat.channelCount > 2) {
        return {AudioStatusCode::InvalidArgument, 0};
    }
    if (config.shareMode != AudioShareMode::Shared) {
        return {AudioStatusCode::UnsupportedMode, 0};
    }

    ComPtr<IMMDevice> device;
    AudioStatus status = ResolveDevice(direction, config.deviceId, device);
    if (!status) {
        return status;
    }

    HRESULT result = device->Activate(
        __uuidof(IAudioClient),
        CLSCTX_ALL,
        nullptr,
        reinterpret_cast<void**>(prepared.client.GetAddressOf()));
    if (FAILED(result)) {
        return StatusFromHResult(result, AudioStatusCode::InitializationFailed);
    }

    WAVEFORMATEX wave = FloatWaveFormat(config.requestedFormat);
    WAVEFORMATEX* closest = nullptr;
    result = prepared.client->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, &wave, &closest);
    if (closest) {
        CoTaskMemFree(closest);
    }
    if (FAILED(result) && result != AUDCLNT_E_UNSUPPORTED_FORMAT) {
        return StatusFromHResult(result, AudioStatusCode::UnsupportedFormat);
    }

    const REFERENCE_TIME requestedDuration = std::max<REFERENCE_TIME>(
        100'000,
        static_cast<REFERENCE_TIME>(
            10'000'000.0 * static_cast<double>(config.requestedFormat.framesPerBuffer) /
            static_cast<double>(config.requestedFormat.sampleRate)));
    const DWORD flags =
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
        AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
        AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
    bool initialized = false;
    if (config.lowLatency) {
        ComPtr<IAudioClient3> client3;
        if (SUCCEEDED(prepared.client.As(&client3))) {
            UINT32 defaultPeriod = 0;
            UINT32 fundamentalPeriod = 0;
            UINT32 minimumPeriod = 0;
            UINT32 maximumPeriod = 0;
            if (SUCCEEDED(client3->GetSharedModeEnginePeriod(
                    &wave,
                    &defaultPeriod,
                    &fundamentalPeriod,
                    &minimumPeriod,
                    &maximumPeriod))) {
                UINT32 period = std::clamp(
                    config.requestedFormat.framesPerBuffer,
                    minimumPeriod,
                    maximumPeriod);
                if (fundamentalPeriod > 0 && period > minimumPeriod) {
                    const UINT32 steps =
                        (period - minimumPeriod + fundamentalPeriod - 1) /
                        fundamentalPeriod;
                    period = std::min(maximumPeriod, minimumPeriod + steps * fundamentalPeriod);
                }
                result = client3->InitializeSharedAudioStream(
                    flags, period, &wave, nullptr);
                initialized = SUCCEEDED(result);
            }
        }
    }
    if (!initialized) {
        result = prepared.client->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            flags,
            requestedDuration,
            0,
            &wave,
            nullptr);
    }
    if (FAILED(result)) {
        return StatusFromHResult(result, AudioStatusCode::InitializationFailed);
    }

    result = prepared.client->GetBufferSize(&prepared.endpointFrames);
    if (FAILED(result)) {
        return StatusFromHResult(result, AudioStatusCode::InitializationFailed);
    }
    REFERENCE_TIME streamLatency = 0;
    if (SUCCEEDED(prepared.client->GetStreamLatency(&streamLatency))) {
        const double milliseconds = static_cast<double>(streamLatency) / 10'000.0;
        if (direction == AudioDeviceDirection::Input) {
            prepared.latency.captureMilliseconds = milliseconds;
        } else {
            prepared.latency.outputMilliseconds = milliseconds;
        }
    }
    const double endpointBufferMilliseconds =
        1000.0 * static_cast<double>(prepared.endpointFrames) /
        static_cast<double>(config.requestedFormat.sampleRate);
    if (direction == AudioDeviceDirection::Input &&
        prepared.latency.captureMilliseconds <= 0.0) {
        prepared.latency.captureMilliseconds = endpointBufferMilliseconds;
    }
    if (direction == AudioDeviceDirection::Output &&
        prepared.latency.outputMilliseconds <= 0.0) {
        prepared.latency.outputMilliseconds = endpointBufferMilliseconds;
    }
    prepared.format = config.requestedFormat;
    prepared.format.framesPerBuffer = prepared.endpointFrames;
    return {};
}

HANDLE PromoteAudioThread() noexcept {
    DWORD taskIndex = 0;
    HANDLE task = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
    if (task) {
        AvSetMmThreadPriority(task, AVRT_PRIORITY_HIGH);
    }
    return task;
}

class WasapiInputStream final : public IAudioInputStream {
public:
    WasapiInputStream(PreparedClient prepared, ComPtr<IAudioCaptureClient> capture, HANDLE event)
        : m_Client(std::move(prepared.client)),
          m_Capture(std::move(capture)),
          m_Event(event),
          m_Format(prepared.format),
          m_Latency(prepared.latency),
          m_Buffer(m_Format.channelCount, prepared.endpointFrames) {}

    ~WasapiInputStream() override {
        Stop();
        if (m_Event) {
            CloseHandle(m_Event);
        }
    }

    AudioStatus Start(AudioInputCallback callback, void* userData) override {
        if (!callback || m_State.load(std::memory_order_acquire) != AudioStreamState::Stopped) {
            return {AudioStatusCode::InvalidArgument, 0};
        }
        m_Callback = callback;
        m_UserData = userData;
        m_StopRequested.store(false, std::memory_order_release);
        m_State.store(AudioStreamState::Starting, std::memory_order_release);
        m_Thread = std::thread([this] { Run(); });
        return {};
    }

    AudioStatus Stop() override {
        if (!m_Thread.joinable()) {
            m_State.store(AudioStreamState::Stopped, std::memory_order_release);
            return {};
        }
        m_StopRequested.store(true, std::memory_order_release);
        SetEvent(m_Event);
        m_Thread.join();
        if (m_State.load(std::memory_order_acquire) != AudioStreamState::DeviceLost) {
            m_State.store(AudioStreamState::Stopped, std::memory_order_release);
        }
        return {};
    }

    void Reset() noexcept override {
        if (m_State.load(std::memory_order_acquire) == AudioStreamState::Stopped) {
            m_Client->Reset();
            m_StreamFrame = 0;
        }
    }
    [[nodiscard]] AudioStreamState State() const noexcept override {
        return m_State.load(std::memory_order_acquire);
    }
    [[nodiscard]] AudioFormat Format() const noexcept override { return m_Format; }
    [[nodiscard]] AudioLatency Latency() const noexcept override { return m_Latency; }

private:
    void Fail(HRESULT result) noexcept {
        m_PlatformError.store(result, std::memory_order_release);
        m_State.store(
            StatusFromHResult(result, AudioStatusCode::PlatformError).code == AudioStatusCode::DeviceLost
                ? AudioStreamState::DeviceLost
                : AudioStreamState::Error,
            std::memory_order_release);
    }

    void Run() noexcept {
        ScopedCom com;
        HANDLE avrtTask = PromoteAudioThread();
        HRESULT result = m_Client->Start();
        if (FAILED(result)) {
            Fail(result);
            return;
        }
        m_State.store(AudioStreamState::Running, std::memory_order_release);
        while (!m_StopRequested.load(std::memory_order_acquire)) {
            const DWORD waitResult = WaitForSingleObject(m_Event, 2000);
            if (waitResult != WAIT_OBJECT_0) {
                if (waitResult == WAIT_FAILED) {
                    Fail(HRESULT_FROM_WIN32(GetLastError()));
                    break;
                }
                continue;
            }
            if (m_StopRequested.load(std::memory_order_acquire)) {
                break;
            }

            UINT32 packetFrames = 0;
            result = m_Capture->GetNextPacketSize(&packetFrames);
            while (SUCCEEDED(result) && packetFrames > 0) {
                BYTE* data = nullptr;
                DWORD flags = 0;
                UINT64 devicePosition = 0;
                result = m_Capture->GetBuffer(
                    &data, &packetFrames, &flags, &devicePosition, nullptr);
                if (FAILED(result)) {
                    Fail(result);
                    break;
                }

                if (packetFrames <= m_Buffer.FrameCount()) {
                    AudioBufferView view = m_Buffer.View();
                    if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0 || data == nullptr) {
                        for (std::size_t channel = 0; channel < view.ChannelCount(); ++channel) {
                            std::fill_n(view.Channel(channel).data(), packetFrames, 0.0f);
                        }
                    } else {
                        const float* interleaved = reinterpret_cast<const float*>(data);
                        for (UINT32 frame = 0; frame < packetFrames; ++frame) {
                            for (std::size_t channel = 0; channel < view.ChannelCount(); ++channel) {
                                view.Sample(channel, frame) =
                                    interleaved[frame * view.ChannelCount() + channel];
                            }
                        }
                    }
                    const AudioBuffer& constantBuffer = m_Buffer;
                    ConstAudioBufferView full = constantBuffer.View();
                    ConstAudioBufferView packet(
                        full.Channel(0).data(),
                        full.ChannelCount(),
                        packetFrames,
                        full.FrameCount());
                    m_Callback(m_UserData, packet, m_StreamFrame);
                    m_StreamFrame += packetFrames;
                }
                m_Capture->ReleaseBuffer(packetFrames);
                if (FAILED(result)) {
                    break;
                }
                result = m_Capture->GetNextPacketSize(&packetFrames);
            }
            if (FAILED(result)) {
                Fail(result);
                break;
            }
        }
        m_Client->Stop();
        if (avrtTask) {
            AvRevertMmThreadCharacteristics(avrtTask);
        }
    }

    ComPtr<IAudioClient> m_Client;
    ComPtr<IAudioCaptureClient> m_Capture;
    HANDLE m_Event = nullptr;
    AudioFormat m_Format{};
    AudioLatency m_Latency{};
    AudioBuffer m_Buffer;
    AudioInputCallback m_Callback = nullptr;
    void* m_UserData = nullptr;
    std::thread m_Thread;
    std::atomic<AudioStreamState> m_State{AudioStreamState::Stopped};
    std::atomic<bool> m_StopRequested{false};
    std::atomic<HRESULT> m_PlatformError{S_OK};
    std::uint64_t m_StreamFrame = 0;
};

class WasapiOutputStream final : public IAudioOutputStream {
public:
    WasapiOutputStream(PreparedClient prepared, ComPtr<IAudioRenderClient> render, HANDLE event)
        : m_Client(std::move(prepared.client)),
          m_Render(std::move(render)),
          m_Event(event),
          m_Format(prepared.format),
          m_Latency(prepared.latency),
          m_Buffer(m_Format.channelCount, prepared.endpointFrames),
          m_EndpointFrames(prepared.endpointFrames) {}

    ~WasapiOutputStream() override {
        Stop();
        if (m_Event) {
            CloseHandle(m_Event);
        }
    }

    AudioStatus Start(AudioOutputCallback callback, void* userData) override {
        if (!callback || m_State.load(std::memory_order_acquire) != AudioStreamState::Stopped) {
            return {AudioStatusCode::InvalidArgument, 0};
        }
        m_Callback = callback;
        m_UserData = userData;
        m_StopRequested.store(false, std::memory_order_release);
        m_State.store(AudioStreamState::Starting, std::memory_order_release);
        m_Thread = std::thread([this] { Run(); });
        return {};
    }

    AudioStatus Stop() override {
        if (!m_Thread.joinable()) {
            m_State.store(AudioStreamState::Stopped, std::memory_order_release);
            return {};
        }
        m_StopRequested.store(true, std::memory_order_release);
        SetEvent(m_Event);
        m_Thread.join();
        if (m_State.load(std::memory_order_acquire) != AudioStreamState::DeviceLost) {
            m_State.store(AudioStreamState::Stopped, std::memory_order_release);
        }
        return {};
    }

    void Reset() noexcept override {
        if (m_State.load(std::memory_order_acquire) == AudioStreamState::Stopped) {
            m_Client->Reset();
            m_StreamFrame = 0;
        }
    }
    [[nodiscard]] AudioStreamState State() const noexcept override {
        return m_State.load(std::memory_order_acquire);
    }
    [[nodiscard]] AudioFormat Format() const noexcept override { return m_Format; }
    [[nodiscard]] AudioLatency Latency() const noexcept override { return m_Latency; }

private:
    void Fail(HRESULT result) noexcept {
        m_PlatformError.store(result, std::memory_order_release);
        m_State.store(
            StatusFromHResult(result, AudioStatusCode::PlatformError).code == AudioStatusCode::DeviceLost
                ? AudioStreamState::DeviceLost
                : AudioStreamState::Error,
            std::memory_order_release);
    }

    void Run() noexcept {
        ScopedCom com;
        HANDLE avrtTask = PromoteAudioThread();
        HRESULT result = m_Client->Start();
        if (FAILED(result)) {
            Fail(result);
            return;
        }
        m_State.store(AudioStreamState::Running, std::memory_order_release);
        while (!m_StopRequested.load(std::memory_order_acquire)) {
            const DWORD waitResult = WaitForSingleObject(m_Event, 2000);
            if (waitResult != WAIT_OBJECT_0) {
                if (waitResult == WAIT_FAILED) {
                    Fail(HRESULT_FROM_WIN32(GetLastError()));
                    break;
                }
                continue;
            }
            if (m_StopRequested.load(std::memory_order_acquire)) {
                break;
            }

            UINT32 padding = 0;
            result = m_Client->GetCurrentPadding(&padding);
            if (FAILED(result)) {
                Fail(result);
                break;
            }
            const UINT32 available = m_EndpointFrames - padding;
            if (available == 0) {
                continue;
            }
            BYTE* data = nullptr;
            result = m_Render->GetBuffer(available, &data);
            if (FAILED(result)) {
                Fail(result);
                break;
            }

            AudioBufferView full = m_Buffer.View();
            AudioBufferView block(
                full.Channel(0).data(),
                full.ChannelCount(),
                available,
                full.FrameCount());
            block.Clear();
            m_Callback(m_UserData, block, m_StreamFrame);
            float* interleaved = reinterpret_cast<float*>(data);
            for (UINT32 frame = 0; frame < available; ++frame) {
                for (std::size_t channel = 0; channel < block.ChannelCount(); ++channel) {
                    interleaved[frame * block.ChannelCount() + channel] =
                        block.Sample(channel, frame);
                }
            }
            result = m_Render->ReleaseBuffer(available, 0);
            if (FAILED(result)) {
                Fail(result);
                break;
            }
            m_StreamFrame += available;
        }
        m_Client->Stop();
        if (avrtTask) {
            AvRevertMmThreadCharacteristics(avrtTask);
        }
    }

    ComPtr<IAudioClient> m_Client;
    ComPtr<IAudioRenderClient> m_Render;
    HANDLE m_Event = nullptr;
    AudioFormat m_Format{};
    AudioLatency m_Latency{};
    AudioBuffer m_Buffer;
    UINT32 m_EndpointFrames = 0;
    AudioOutputCallback m_Callback = nullptr;
    void* m_UserData = nullptr;
    std::thread m_Thread;
    std::atomic<AudioStreamState> m_State{AudioStreamState::Stopped};
    std::atomic<bool> m_StopRequested{false};
    std::atomic<HRESULT> m_PlatformError{S_OK};
    std::uint64_t m_StreamFrame = 0;
};

template<class Service>
AudioStatus FinishOpen(
    PreparedClient& prepared,
    ComPtr<Service>& service,
    HANDLE& event) {
    event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event) {
        return {AudioStatusCode::InitializationFailed, GetLastError()};
    }
    HRESULT result = prepared.client->SetEventHandle(event);
    if (SUCCEEDED(result)) {
        result = prepared.client->GetService(IID_PPV_ARGS(&service));
    }
    if (FAILED(result)) {
        CloseHandle(event);
        event = nullptr;
        return StatusFromHResult(result, AudioStatusCode::InitializationFailed);
    }
    return {};
}

} // namespace

WasapiBackend::WasapiBackend() noexcept {
    const HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    m_ComInitialized = SUCCEEDED(result);
}

WasapiBackend::~WasapiBackend() {
    if (m_ComInitialized) {
        CoUninitialize();
    }
}

std::vector<AudioDeviceInfo> WasapiBackend::EnumerateDevices(
    AudioDeviceDirection direction,
    AudioStatus* status) {
    ScopedCom com;
    if (!com.Available()) {
        if (status) {
            *status = StatusFromHResult(com.Result(), AudioStatusCode::BackendUnavailable);
        }
        return {};
    }
    ComPtr<IMMDeviceEnumerator> enumerator;
    AudioStatus local = CreateEnumerator(enumerator);
    if (!local) {
        if (status) {
            *status = local;
        }
        return {};
    }

    AudioDeviceId defaultId;
    ComPtr<IMMDevice> defaultDevice;
    if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(Flow(direction), eConsole, &defaultDevice))) {
        LPWSTR id = nullptr;
        if (SUCCEEDED(defaultDevice->GetId(&id))) {
            defaultId = id;
            CoTaskMemFree(id);
        }
    }

    ComPtr<IMMDeviceCollection> collection;
    HRESULT result = enumerator->EnumAudioEndpoints(Flow(direction), DEVICE_STATE_ACTIVE, &collection);
    if (FAILED(result)) {
        if (status) {
            *status = StatusFromHResult(result, AudioStatusCode::PlatformError);
        }
        return {};
    }

    UINT count = 0;
    collection->GetCount(&count);
    std::vector<AudioDeviceInfo> devices;
    devices.reserve(count);
    for (UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> device;
        if (FAILED(collection->Item(i, &device))) {
            continue;
        }

        LPWSTR id = nullptr;
        if (FAILED(device->GetId(&id))) {
            continue;
        }
        AudioDeviceInfo info;
        info.id = id;
        info.direction = direction;
        info.isDefault = info.id == defaultId;
        CoTaskMemFree(id);

        ComPtr<IPropertyStore> properties;
        if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &properties))) {
            PROPVARIANT value;
            PropVariantInit(&value);
            if (SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &value)) &&
                value.vt == VT_LPWSTR && value.pwszVal) {
                info.name = value.pwszVal;
            }
            PropVariantClear(&value);
        }
        if (info.name.empty()) {
            info.name = L"(unnamed endpoint)";
        }

        ComPtr<IAudioClient> client;
        if (SUCCEEDED(device->Activate(
                __uuidof(IAudioClient),
                CLSCTX_ALL,
                nullptr,
                reinterpret_cast<void**>(client.GetAddressOf())))) {
            WAVEFORMATEX* mix = nullptr;
            if (SUCCEEDED(client->GetMixFormat(&mix)) && mix) {
                info.mixFormat = ToAudioFormat(*mix, 0);
                CoTaskMemFree(mix);
            }
        }
        devices.push_back(std::move(info));
    }
    if (status) {
        *status = {};
    }
    return devices;
}

std::unique_ptr<IAudioInputStream> WasapiBackend::OpenInput(
    const AudioStreamConfig& config,
    AudioStatus* status) {
    ScopedCom com;
    if (!com.Available()) {
        if (status) {
            *status = StatusFromHResult(com.Result(), AudioStatusCode::BackendUnavailable);
        }
        return {};
    }
    PreparedClient prepared;
    AudioStatus local = PrepareClient(AudioDeviceDirection::Input, config, prepared);
    ComPtr<IAudioCaptureClient> capture;
    HANDLE event = nullptr;
    if (local) {
        local = FinishOpen(prepared, capture, event);
    }
    if (status) {
        *status = local;
    }
    if (!local) {
        return {};
    }
    return std::make_unique<WasapiInputStream>(std::move(prepared), std::move(capture), event);
}

std::unique_ptr<IAudioOutputStream> WasapiBackend::OpenOutput(
    const AudioStreamConfig& config,
    AudioStatus* status) {
    ScopedCom com;
    if (!com.Available()) {
        if (status) {
            *status = StatusFromHResult(com.Result(), AudioStatusCode::BackendUnavailable);
        }
        return {};
    }
    PreparedClient prepared;
    AudioStatus local = PrepareClient(AudioDeviceDirection::Output, config, prepared);
    ComPtr<IAudioRenderClient> render;
    HANDLE event = nullptr;
    if (local) {
        local = FinishOpen(prepared, render, event);
    }
    if (status) {
        *status = local;
    }
    if (!local) {
        return {};
    }
    return std::make_unique<WasapiOutputStream>(std::move(prepared), std::move(render), event);
}

} // namespace Engine::Audio
