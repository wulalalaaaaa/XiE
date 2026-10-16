#include "App/VoiceMixerAudioController.h"
#include "Audio/DSP/Noise/NoiseFrameAdapter.h"
#include "Audio/DSP/Noise/NoiseSuppressorProcessor.h"
#include "Audio/DSP/Noise/RNNoiseSuppressor.h"
#include "Input/InputEventQueue.h"
#include "Platform/Win32/Win32ApplicationHost.h"
#include "Platform/Win32/Win32WindowSystem.h"
#include "Renderer2D/Backends/Software/LayeredWindowPresenter.h"
#include "Renderer2D/Backends/Software/LayeredWindowRenderPipeline.h"
#include "Renderer2D/Backends/Software/Software2DRenderer.h"
#include "Preset/VoicePreset.h"
#include "UI/VoiceMixerWindow.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <CommCtrl.h>
#include <Shellapi.h>
#include <windowsx.h>
#ifdef CreateWindow
#undef CreateWindow
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

constexpr UINT kTrayMessage = WM_APP + 17;
constexpr UINT_PTR kWindowSubclassId = 0x58494556;
constexpr UINT kTrayIconId = 1;
constexpr UINT kTrayOpen = 1001;
constexpr UINT kTrayExit = 1002;
constexpr float kInitialWidth = 1400.0f;
constexpr float kInitialHeight = 900.0f;
constexpr float kMinimumWidth = 1040.0f;
constexpr float kMinimumHeight = 680.0f;

struct WindowBridge {
    bool exitRequested = false;
    bool chinese = true;
};

std::filesystem::path PresetPath() {
    wchar_t localAppData[32768]{};
    const DWORD size = GetEnvironmentVariableW(
        L"LOCALAPPDATA", localAppData, static_cast<DWORD>(std::size(localAppData)));
    std::filesystem::path root = size > 0 && size < std::size(localAppData)
        ? std::filesystem::path(localAppData)
        : std::filesystem::temp_directory_path();
    const std::filesystem::path mixerRoot = root / "XiE" / "VoiceMixer";
    const std::filesystem::path legacyRoot = root / "XiE" / "VoiceChanger";
    std::error_code error;
    std::filesystem::create_directories(mixerRoot / "Presets", error);
    const auto migrate = [&](const std::filesystem::path& relative) {
        const std::filesystem::path destination = mixerRoot / relative;
        const std::filesystem::path source = legacyRoot / relative;
        error.clear();
        if (!std::filesystem::exists(destination, error) &&
            std::filesystem::exists(source, error)) {
            std::filesystem::copy_file(source, destination,
                std::filesystem::copy_options::skip_existing, error);
        }
    };
    migrate(std::filesystem::path("Presets") / "Current.json");
    migrate("Language.txt");
    migrate("LastEditorPage.txt");
    return mixerRoot / "Presets" / "Current.json";
}

bool HasArgument(std::wstring_view argument) {
    return std::wstring_view(GetCommandLineW()).find(argument) != std::wstring_view::npos;
}

float DpiScale(HWND window) {
    return std::max(1.0f, static_cast<float>(GetDpiForWindow(window)) / 96.0f);
}

void RestoreWindow(HWND window) {
    ::ShowWindow(window, SW_RESTORE);
    ::ShowWindow(window, SW_SHOW);
    SetForegroundWindow(window);
}

void ShowTrayMenu(HWND window, WindowBridge& bridge) {
    POINT cursor{};
    GetCursorPos(&cursor);
    HMENU menu = CreatePopupMenu();
    if (menu == nullptr) return;
    AppendMenuW(menu, MF_STRING, kTrayOpen,
        bridge.chinese ? L"打开 XiE Voice Mixer" : L"Open XiE Voice Mixer");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kTrayExit, bridge.chinese ? L"退出" : L"Exit");
    SetForegroundWindow(window);
    const UINT command = TrackPopupMenu(menu,
        TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
        cursor.x, cursor.y, 0, window, nullptr);
    DestroyMenu(menu);
    if (command == kTrayOpen) {
        RestoreWindow(window);
    } else if (command == kTrayExit) {
        bridge.exitRequested = true;
    }
}

LRESULT CALLBACK VoiceMixerSubclass(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    UINT_PTR,
    DWORD_PTR reference) {
    auto& bridge = *reinterpret_cast<WindowBridge*>(reference);
    switch (message) {
    case WM_CLOSE:
        ::ShowWindow(window, SW_HIDE);
        return 0;
    case WM_GETMINMAXINFO: {
        auto* limits = reinterpret_cast<MINMAXINFO*>(lParam);
        const float dpi = DpiScale(window);
        limits->ptMinTrackSize.x = static_cast<LONG>(std::lround(kMinimumWidth * dpi));
        limits->ptMinTrackSize.y = static_cast<LONG>(std::lround(kMinimumHeight * dpi));
        return 0;
    }
    case WM_DPICHANGED: {
        const auto* suggested = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(window, nullptr,
            suggested->left, suggested->top,
            suggested->right - suggested->left,
            suggested->bottom - suggested->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_NCHITTEST: {
        const LRESULT inherited = DefSubclassProc(window, message, wParam, lParam);
        if (inherited != HTCLIENT) return inherited;
        RECT rect{};
        GetWindowRect(window, &rect);
        const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const int border = std::max(7, static_cast<int>(std::lround(7.0f * DpiScale(window))));
        const bool left = point.x < rect.left + border;
        const bool right = point.x >= rect.right - border;
        const bool top = point.y < rect.top + border;
        const bool bottom = point.y >= rect.bottom - border;
        if (top && left) return HTTOPLEFT;
        if (top && right) return HTTOPRIGHT;
        if (bottom && left) return HTBOTTOMLEFT;
        if (bottom && right) return HTBOTTOMRIGHT;
        if (left) return HTLEFT;
        if (right) return HTRIGHT;
        if (top) return HTTOP;
        if (bottom) return HTBOTTOM;
        return HTCLIENT;
    }
    default:
        break;
    }

    if (message == kTrayMessage) {
        const UINT notification = LOWORD(lParam);
        if (notification == WM_LBUTTONUP || notification == WM_LBUTTONDBLCLK ||
            notification == NIN_SELECT || notification == NIN_KEYSELECT) {
            RestoreWindow(window);
        } else if (notification == WM_RBUTTONUP || notification == WM_CONTEXTMENU) {
            ShowTrayMenu(window, bridge);
        }
        return 0;
    }
    return DefSubclassProc(window, message, wParam, lParam);
}

class TrayIcon {
public:
    TrayIcon(HWND window, bool enabled) : m_Window(window), m_Enabled(enabled) {
        if (!m_Enabled) return;
        m_Data.cbSize = sizeof(m_Data);
        m_Data.hWnd = window;
        m_Data.uID = kTrayIconId;
        m_Data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        m_Data.uCallbackMessage = kTrayMessage;
        m_Data.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(32512));
        wcscpy_s(m_Data.szTip, L"XiE Voice Mixer");
        if (Shell_NotifyIconW(NIM_ADD, &m_Data)) {
            m_Data.uVersion = NOTIFYICON_VERSION_4;
            (void)Shell_NotifyIconW(NIM_SETVERSION, &m_Data);
            m_Added = true;
        }
    }

    ~TrayIcon() {
        if (m_Added) (void)Shell_NotifyIconW(NIM_DELETE, &m_Data);
    }

    [[nodiscard]] bool Added() const noexcept { return m_Added; }

private:
    HWND m_Window = nullptr;
    bool m_Enabled = false;
    bool m_Added = false;
    NOTIFYICONDATAW m_Data{};
};

void ConfigureInitialWindow(HWND window) {
    const float dpi = DpiScale(window);
    HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{sizeof(info)};
    GetMonitorInfoW(monitor, &info);
    const float workWidth = static_cast<float>(info.rcWork.right - info.rcWork.left) / dpi;
    const float workHeight = static_cast<float>(info.rcWork.bottom - info.rcWork.top) / dpi;
    const float logicalWidth = std::max(kMinimumWidth, std::min(kInitialWidth, workWidth * 0.92f));
    const float logicalHeight = std::max(kMinimumHeight, std::min(kInitialHeight, workHeight * 0.92f));
    const int width = static_cast<int>(std::lround(logicalWidth * dpi));
    const int height = static_cast<int>(std::lround(logicalHeight * dpi));
    const int x = info.rcWork.left + (info.rcWork.right - info.rcWork.left - width) / 2;
    const int y = info.rcWork.top + (info.rcWork.bottom - info.rcWork.top - height) / 2;
    SetWindowPos(window, nullptr, x, y, width, height,
        SWP_NOZORDER | SWP_NOACTIVATE);
}

bool RunNoiseSelfTest() {
    using namespace Engine::Audio;
    using namespace XiE::VoiceMixer;

    const AudioFormat format{48'000, 1, AudioSampleFormat::Float32, 128};
    RNNoiseSuppressor algorithm;
    if (!algorithm.Prepare({format}) || algorithm.RequiredFrameSize() == 0) return false;

    NoiseFrameAdapter adapter;
    if (!adapter.Prepare(algorithm.RequiredFrameSize())) return false;
    std::vector<float> input(64, 0.0f);
    std::vector<float> delayedDry(64, 0.0f);
    std::vector<float> processed(64, 0.0f);
    for (int block = 0; block < 12; ++block) {
        if (!adapter.Process(input, delayedDry, processed, algorithm).valid) return false;
    }
    if (adapter.CaptureMetrics().inputFrames != 12u * input.size()) return false;

    NoiseSuppressorProcessor processor;
    if (!processor.Prepare(format)) return false;
    NoiseSuppressorConfig noise;
    noise.enabled = true;
    if (!processor.Publish(BuildNoiseSuppressorSnapshot(noise, 1))) return false;
    AudioBuffer buffer(1, format.framesPerBuffer);
    AudioProcessContext context{buffer.View(), buffer.View(), format.sampleRate, 0};
    for (int block = 0; block < 8; ++block) processor.Process(context);
    const auto metrics = processor.CaptureMetrics();
    if (!metrics.active || metrics.frameSize != algorithm.RequiredFrameSize()) return false;
    for (float sample : buffer.View().Channel(0)) {
        if (!std::isfinite(sample)) return false;
    }

    const std::filesystem::path root = std::filesystem::temp_directory_path() /
        ("XiEVoiceMixer-selftest-" + std::to_string(GetCurrentProcessId()));
    const std::filesystem::path presetPath = root / "legacy-v1.json";
    std::error_code fsError;
    std::filesystem::create_directories(root, fsError);
    if (fsError) return false;
    {
        std::ofstream output(presetPath, std::ios::binary | std::ios::trunc);
        output << R"({
  "schemaVersion": 1,
  "name": "Legacy",
  "parameters": {},
  "pitchCurve": [{"id":1,"x":70,"y":0,"interpolation":"smooth"}],
  "formantCurve": [{"id":1,"x":70,"y":0,"interpolation":"smooth"}]
})";
        if (!output) return false;
    }
    VoicePresetRepository repository;
    VoicePreset legacy;
    std::string error;
    const bool migrated = repository.Load(presetPath, legacy, error) &&
        legacy.eq.Bands().empty() && !legacy.parameters.noiseEnabled &&
        legacy.parameters.noiseStrength == 1.0f && legacy.parameters.noiseWet == 1.0f;
    std::filesystem::remove_all(root, fsError);
    return migrated;
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    using namespace Engine;
    using namespace XiE::VoiceMixer;
    const bool selfTest = HasArgument(L"--self-test");
    const bool noiseSelfTestPassed = !selfTest || RunNoiseSelfTest();

    try {
        Win32ApplicationHost host;
        Win32WindowSystem windows;
        WindowDesc description;
        description.title = "XiE Voice Mixer";
        description.width = static_cast<int>(kInitialWidth);
        description.height = static_cast<int>(kInitialHeight);
        description.visible = false;
        description.resizable = true;
        description.decorated = false;
        description.transparent = true;

        const WindowHandle window = windows.CreateWindow(description);
        const HWND nativeWindow = reinterpret_cast<HWND>(windows.GetNativeHandle(window));
        ConfigureInitialWindow(nativeWindow);

        WindowBridge bridge;
        if (!SetWindowSubclass(nativeWindow, VoiceMixerSubclass,
                kWindowSubclassId, reinterpret_cast<DWORD_PTR>(&bridge))) {
            throw std::runtime_error("Failed to initialize the application window");
        }
        TrayIcon tray(nativeWindow, !selfTest);
        if (!selfTest && !tray.Added()) {
            throw std::runtime_error("Failed to create the notification-area icon");
        }

        InputEventQueue input(window, 1024);
        windows.SetInputEventQueue(window, &input);

        VoiceMixerAudioController audio;
        if (!selfTest) {
            (void)audio.RefreshDevices();
        }
        VoiceMixerWindow view(audio, PresetPath());
        bridge.chinese = view.IsChineseLanguage();
        Software2DRenderer renderer(nullptr, &view.TextLayouts());
        LayeredWindowPresenter presenter(windows.GetNativeHandle(window));
        LayeredWindowRenderPipeline pipeline(renderer, presenter);
        DrawList2D drawList;

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        float dpi = 1.0f;
        auto updateViewport = [&] {
            RECT client{};
            GetClientRect(nativeWindow, &client);
            dpi = DpiScale(nativeWindow);
            framebufferWidth = std::max(1, static_cast<int>(client.right - client.left));
            framebufferHeight = std::max(1, static_cast<int>(client.bottom - client.top));
            const float logicalWidth = static_cast<float>(framebufferWidth) / dpi;
            const float logicalHeight = static_cast<float>(framebufferHeight) / dpi;
            view.SetViewport(logicalWidth, logicalHeight, dpi);
            pipeline.Resize(
                static_cast<int>(std::lround(logicalWidth)),
                static_cast<int>(std::lround(logicalHeight)), dpi);
        };

        updateViewport();
        view.Tick();
        view.BuildDrawList(drawList);
        const Render2DStats initialStats = pipeline.Render(drawList, {
            static_cast<float>(framebufferWidth), static_cast<float>(framebufferHeight), dpi});
        const bool initialPresentSucceeded = presenter.Present(renderer.GetBitmap());
        if (selfTest) {
            SetWindowPos(nativeWindow, nullptr, 0, 0,
                static_cast<int>(std::lround(kMinimumWidth * dpi)),
                static_cast<int>(std::lround(kMinimumHeight * dpi)),
                SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            updateViewport();
            view.BuildDrawList(drawList);
            const Render2DStats minimumStats = pipeline.Render(drawList, {
                static_cast<float>(framebufferWidth), static_cast<float>(framebufferHeight), dpi});
            const bool minimumPresentSucceeded = presenter.Present(renderer.GetBitmap());
            windows.SetInputEventQueue(window, nullptr);
            RemoveWindowSubclass(nativeWindow, VoiceMixerSubclass, kWindowSubclassId);
            windows.DestroyWindow(window);
            return noiseSelfTestPassed && initialStats.commandCount > 180 && initialPresentSucceeded &&
                minimumStats.commandCount > 180 && minimumPresentSucceeded ? 0 : 2;
        }
        windows.ShowWindow(window);

        auto nextFrame = std::chrono::steady_clock::now();
        int previousWidth = framebufferWidth;
        int previousHeight = framebufferHeight;
        float previousDpi = dpi;
        while (!host.ShouldExit() && !bridge.exitRequested) {
            host.PollEvents();

            RECT client{};
            GetClientRect(nativeWindow, &client);
            const int currentWidth = std::max(1, static_cast<int>(client.right - client.left));
            const int currentHeight = std::max(1, static_cast<int>(client.bottom - client.top));
            const float currentDpi = DpiScale(nativeWindow);
            if (currentWidth != previousWidth || currentHeight != previousHeight ||
                std::abs(currentDpi - previousDpi) >= 0.01f) {
                updateViewport();
                previousWidth = framebufferWidth;
                previousHeight = framebufferHeight;
                previousDpi = dpi;
            }

            for (const InputEvent& event : input.Drain()) {
                if (const auto* pointer = std::get_if<PointerButtonEvent>(&event);
                    pointer != nullptr && pointer->pressed &&
                    pointer->button == PointerButton::Primary &&
                    view.IsTitleBarDragRegion(pointer->logicalPosition)) {
                    ReleaseCapture();
                    SendMessageW(nativeWindow, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                }
                view.HandleInput(event);
            }
            if (view.ConsumeHideRequested()) {
                ::ShowWindow(nativeWindow, SW_HIDE);
            }
            if (view.ConsumeMinimizeRequested()) {
                ::ShowWindow(nativeWindow, SW_MINIMIZE);
            }

            view.Tick();
            bridge.chinese = view.IsChineseLanguage();
            if (IsWindowVisible(nativeWindow) != FALSE && !IsIconic(nativeWindow)) {
                view.BuildDrawList(drawList);
                (void)pipeline.Render(drawList, {
                    static_cast<float>(framebufferWidth), static_cast<float>(framebufferHeight), dpi});
                pipeline.Present();
            }
            nextFrame += std::chrono::milliseconds(33);
            const auto now = std::chrono::steady_clock::now();
            if (nextFrame > now) {
                host.WaitForEventsTimeout(
                    std::chrono::duration<double>(nextFrame - now).count());
            } else {
                nextFrame = now;
            }
        }
        (void)audio.Stop();
        windows.SetInputEventQueue(window, nullptr);
        RemoveWindowSubclass(nativeWindow, VoiceMixerSubclass, kWindowSubclassId);
        windows.DestroyWindow(window);
        return 0;
    } catch (const std::exception& exception) {
        MessageBoxA(nullptr, exception.what(), "XiE Voice Mixer", MB_OK | MB_ICONERROR);
        return 1;
    }
}
