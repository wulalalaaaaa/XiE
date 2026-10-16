#include "Application/DesktopServices.h"

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <shellapi.h>

#include <algorithm>
#include <atomic>
#include <aclapi.h>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Engine {
namespace {

constexpr UINT kTrayCallbackMessage = WM_APP + 0x431u;
constexpr std::uint32_t kActivationMagic = 0x31414958u; // XIA1
constexpr std::uint16_t kActivationVersion = 1;
constexpr std::uint16_t kActivatePrimary = 1;
constexpr std::uint16_t kActivationAccepted = 0;
constexpr std::uint16_t kActivationRejected = 1;
constexpr wchar_t kWindowClass[] = L"XiE.DesktopServices.MessageHost.v1";

#pragma pack(push, 1)
struct ActivationRequest {
    std::uint32_t magic = kActivationMagic;
    std::uint16_t version = kActivationVersion;
    std::uint16_t command = kActivatePrimary;
    std::uint32_t payloadBytes = 0;
};

struct ActivationResponse {
    std::uint32_t magic = kActivationMagic;
    std::uint16_t version = kActivationVersion;
    std::uint16_t status = kActivationRejected;
};
#pragma pack(pop)

ApplicationError MakeError(ApplicationErrorCode code,
    std::string operation, std::string capability, std::string message,
    std::string backend, DWORD nativeCode = ERROR_SUCCESS) {
    return {code, std::move(operation), std::move(capability),
        std::move(message), nativeCode == ERROR_SUCCESS
            ? std::nullopt
            : std::optional<std::int64_t>(nativeCode), std::move(backend)};
}

std::wstring ToWide(std::string_view text) {
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) return {};
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
            static_cast<int>(text.size()), result.data(), size) <= 0) {
        return {};
    }
    return result;
}

std::uint64_t StableId(std::string_view value) {
    std::uint64_t hash = 1469598103934665603ull;
    for (const unsigned char byte : value) {
        hash ^= byte;
        hash *= 1099511628211ull;
    }
    return hash;
}

std::wstring HexId(std::uint64_t value) {
    wchar_t buffer[17]{};
    (void)swprintf_s(buffer, ARRAYSIZE(buffer), L"%016llX",
        static_cast<unsigned long long>(value));
    return buffer;
}

std::wstring CurrentSessionSuffix() {
    DWORD session = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session)) session = 0;
    return std::to_wstring(session);
}

DesktopEnvironmentStatus ProbeEnvironment(std::string& reason) {
    if (GetShellWindow() == nullptr) {
        reason = "environment:explorer_missing";
        return DesktopEnvironmentStatus::ExplorerMissing;
    }
    if (FindWindowW(L"Shell_TrayWnd", nullptr) == nullptr) {
        reason = "environment:notification_area_unavailable";
        return DesktopEnvironmentStatus::NotificationAreaUnavailable;
    }
    reason.clear();
    return DesktopEnvironmentStatus::Ready;
}

DesktopEnvironmentStatus ClassifyTrayError(DWORD errorCode,
    std::string& reason) {
    switch (errorCode) {
    case ERROR_ACCESS_DENIED:
        reason = "tray_permission_denied";
        return DesktopEnvironmentStatus::PermissionDenied;
    case ERROR_INVALID_PARAMETER:
        reason = "tray_invalid_parameters";
        return DesktopEnvironmentStatus::InvalidParameters;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_TIMEOUT:
        reason = "tray_shell_restarting";
        return DesktopEnvironmentStatus::ShellRestarting;
    default:
        reason = "tray_backend_failure";
        return DesktopEnvironmentStatus::BackendFailure;
    }
}

DesktopEnvironmentStatus ClassifyActivationError(DWORD errorCode,
    std::string& reason) {
    switch (errorCode) {
    case ERROR_ACCESS_DENIED:
        reason = "activation_permission_denied";
        return DesktopEnvironmentStatus::PermissionDenied;
    case ERROR_PIPE_BUSY:
    case ERROR_SEM_TIMEOUT:
    case ERROR_TIMEOUT:
        reason = "activation_endpoint_timeout";
        return DesktopEnvironmentStatus::ShellRestarting;
    case ERROR_INVALID_SID:
    case ERROR_INVALID_SECURITY_DESCR:
        reason = "activation_session_isolated";
        return DesktopEnvironmentStatus::SessionIsolated;
    default:
        reason = "activation_endpoint_backend_failure";
        return DesktopEnvironmentStatus::BackendFailure;
    }
}

CapabilityStatus HealthStatusForEnvironment(
    DesktopEnvironmentStatus status) noexcept {
    switch (status) {
    case DesktopEnvironmentStatus::ExplorerMissing:
    case DesktopEnvironmentStatus::NotificationAreaUnavailable:
    case DesktopEnvironmentStatus::ShellRestarting:
    case DesktopEnvironmentStatus::PermissionDenied:
    case DesktopEnvironmentStatus::SessionIsolated:
        return CapabilityStatus::Unavailable;
    default:
        return CapabilityStatus::Degraded;
    }
}

bool IsModifier(InputModifiers value, InputModifiers flag) {
    return (static_cast<std::uint8_t>(value) &
        static_cast<std::uint8_t>(flag)) != 0;
}

bool ToNativeShortcut(InputKeyCode key, InputModifiers modifiers,
    UINT& nativeModifiers, UINT& nativeKey, std::string& error) {
    nativeModifiers = MOD_NOREPEAT;
    if (IsModifier(modifiers, InputModifiers::Shift)) nativeModifiers |= MOD_SHIFT;
    if (IsModifier(modifiers, InputModifiers::Control)) nativeModifiers |= MOD_CONTROL;
    if (IsModifier(modifiers, InputModifiers::Alt)) nativeModifiers |= MOD_ALT;
    if (IsModifier(modifiers, InputModifiers::Super)) nativeModifiers |= MOD_WIN;

    switch (key) {
    case InputKeyCode::Escape: nativeKey = VK_ESCAPE; return true;
    case InputKeyCode::Enter: nativeKey = VK_RETURN; return true;
    case InputKeyCode::Tab: nativeKey = VK_TAB; return true;
    case InputKeyCode::Backspace: nativeKey = VK_BACK; return true;
    case InputKeyCode::Delete: nativeKey = VK_DELETE; return true;
    case InputKeyCode::Space: nativeKey = VK_SPACE; return true;
    case InputKeyCode::Insert: nativeKey = VK_INSERT; return true;
    case InputKeyCode::Home: nativeKey = VK_HOME; return true;
    case InputKeyCode::End: nativeKey = VK_END; return true;
    case InputKeyCode::PageUp: nativeKey = VK_PRIOR; return true;
    case InputKeyCode::PageDown: nativeKey = VK_NEXT; return true;
    case InputKeyCode::Left: nativeKey = VK_LEFT; return true;
    case InputKeyCode::Right: nativeKey = VK_RIGHT; return true;
    case InputKeyCode::Up: nativeKey = VK_UP; return true;
    case InputKeyCode::Down: nativeKey = VK_DOWN; return true;
    case InputKeyCode::F1: nativeKey = VK_F1; return true;
    case InputKeyCode::F2: nativeKey = VK_F2; return true;
    case InputKeyCode::F3: nativeKey = VK_F3; return true;
    case InputKeyCode::F4: nativeKey = VK_F4; return true;
    case InputKeyCode::F5: nativeKey = VK_F5; return true;
    case InputKeyCode::F6: nativeKey = VK_F6; return true;
    case InputKeyCode::F7: nativeKey = VK_F7; return true;
    case InputKeyCode::F8: nativeKey = VK_F8; return true;
    case InputKeyCode::F9: nativeKey = VK_F9; return true;
    case InputKeyCode::F10: nativeKey = VK_F10; return true;
    case InputKeyCode::F11: nativeKey = VK_F11; return true;
    case InputKeyCode::F12: nativeKey = VK_F12; return true;
    default: break;
    }
    const auto numeric = static_cast<std::uint16_t>(key);
    const auto first = static_cast<std::uint16_t>(InputKeyCode::A);
    const auto last = static_cast<std::uint16_t>(InputKeyCode::Z);
    if (numeric >= first && numeric <= last) {
        nativeKey = static_cast<UINT>('A' + numeric - first);
        return true;
    }
    const auto digitFirst = static_cast<std::uint16_t>(InputKeyCode::Digit0);
    const auto digitLast = static_cast<std::uint16_t>(InputKeyCode::Digit9);
    if (numeric >= digitFirst && numeric <= digitLast) {
        nativeKey = static_cast<UINT>('0' + numeric - digitFirst);
        return true;
    }
    error = "Win32 desktop backend does not support this shortcut key";
    return false;
}

struct TrayNative {
    DesktopRegistrationId registration = 0;
    UINT iconId = 0;
    NOTIFYICONDATAW data{};
    TrayOptions options;
    bool added = false;
};

struct ShortcutNative {
    DesktopRegistrationId registration = 0;
    int nativeId = 0;
};

struct Win32DesktopBackend final : IDesktopBackend {
    struct Impl;

    Win32DesktopBackend() : m_Impl(std::make_unique<Impl>()) {}
    ~Win32DesktopBackend() override { Stop(); }

    DesktopBackendCapabilities Probe() const noexcept override {
        DesktopBackendCapabilities capabilities;
        capabilities.systemTray = true;
        capabilities.globalShortcut = true;
        capabilities.singleInstance = true;
        capabilities.primaryInstanceActivation = true;
        capabilities.backend = "win32.desktop_services.v1";
        capabilities.environment = ProbeEnvironment(
            capabilities.environmentReason);
        return capabilities;
    }

    ApplicationResult<void> Start(
        const DesktopBackendStartOptions& options) override;
    ApplicationResult<DesktopRegistrationId> CreateTray(
        const TrayOptions& options) override;
    ApplicationResult<void> UpdateTray(DesktopRegistrationId registration,
        const std::vector<TrayMenuItem>& menu,
        const std::string& tooltip) override;
    ApplicationResult<void> RemoveTray(
        DesktopRegistrationId registration) override;
    ApplicationResult<DesktopRegistrationId> RegisterGlobalShortcut(
        const ShortcutOptions& options) override;
    ApplicationResult<void> UnregisterGlobalShortcut(
        DesktopRegistrationId registration) override;
    ApplicationResult<SingleInstanceAcquireResult>
        AcquireSingleInstance(const SingleInstanceOptions& options) override;
    ApplicationResult<void> ReleaseSingleInstance(
        DesktopRegistrationId registration) override;
    void Pump() override;
    void Stop() noexcept override;

    std::unique_ptr<Impl> m_Impl;
};

struct Win32DesktopBackend::Impl {
    DesktopBackendStartOptions callbacks;
    std::string error;
    HWND window = nullptr;
    bool started = false;
    bool stopping = false;
    std::uint64_t nextRegistration = 1;
    std::unordered_map<DesktopRegistrationId, TrayNative> trays;
    std::unordered_map<DesktopRegistrationId, ShortcutNative> shortcuts;

    DesktopRegistrationId instanceRegistration = 0;
    HANDLE instanceMutex = nullptr;
    std::wstring instancePipeName;
    std::uint32_t maxActivationBytes = 0;
    HANDLE pipeStopEvent = nullptr;
    HANDLE pipeReadyEvent = nullptr;
    std::atomic<bool> activationEndpointReady = false;
    std::atomic<DWORD> activationEndpointError = ERROR_SUCCESS;
    std::thread pipeThread;

    void Emit(DesktopBackendEvent event) {
        if (callbacks.enqueueEvent) callbacks.enqueueEvent(event);
    }

    void EmitHealth(ApplicationCapability capability, CapabilityStatus status,
        std::string operation, std::string reason,
        std::optional<std::int64_t> nativeError = std::nullopt,
        bool recovering = false) {
        DesktopBackendEvent event;
        event.type = DesktopBackendEventType::HealthChanged;
        event.capability = capability;
        event.healthStatus = status;
        event.operation = std::move(operation);
        event.reason = std::move(reason);
        event.nativeError = nativeError;
        event.recovering = recovering;
        Emit(std::move(event));
    }
};

void ShowTrayMenu(Win32DesktopBackend::Impl& impl, TrayNative& tray);

// A named pipe created by a restricted desktop launcher can have a default
// DACL that rejects the sibling secondary process even when both use the same
// account.  Grant only the current process token's user SID and reject remote
// clients; never broaden the ACL to a public principal.
struct LocalActivationPipeSecurity {
    PACL acl = nullptr;
    SECURITY_DESCRIPTOR descriptor{};
    SECURITY_ATTRIBUTES attributes{};
    HANDLE token = nullptr;
    std::vector<std::uint8_t> userSid;
    std::vector<std::uint8_t> logonSid;

    ~LocalActivationPipeSecurity() {
        if (acl != nullptr) LocalFree(acl);
        if (token != nullptr) CloseHandle(token);
    }

    bool Initialize(DWORD& errorCode) {
        errorCode = ERROR_SUCCESS;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token) == FALSE) {
            errorCode = GetLastError();
            return false;
        }
        DWORD userBytes = 0;
        (void)GetTokenInformation(token, TokenUser, nullptr, 0, &userBytes);
        if (userBytes == 0 || GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            errorCode = GetLastError();
            return false;
        }
        userSid.resize(userBytes);
        if (GetTokenInformation(token, TokenUser, userSid.data(), userBytes,
                &userBytes) == FALSE) {
            errorCode = GetLastError();
            return false;
        }
        const auto* tokenUser = reinterpret_cast<const TOKEN_USER*>(
            userSid.data());
        if (tokenUser->User.Sid == nullptr ||
            IsValidSid(tokenUser->User.Sid) == FALSE) {
            errorCode = ERROR_INVALID_SID;
            return false;
        }
        std::array<std::uint8_t, SECURITY_MAX_SID_SIZE> logonInfo{};
        DWORD logonBytes = static_cast<DWORD>(logonInfo.size());
        PSID logonSessionSid = nullptr;
        if (GetTokenInformation(token, TokenLogonSid, logonInfo.data(),
                logonBytes, &logonBytes) != FALSE) {
            const auto* groups = reinterpret_cast<const TOKEN_GROUPS*>(
                logonInfo.data());
            if (groups->GroupCount > 0 &&
                IsValidSid(groups->Groups[0].Sid) != FALSE) {
                logonSid.assign(reinterpret_cast<const std::uint8_t*>(
                    groups->Groups[0].Sid),
                    reinterpret_cast<const std::uint8_t*>(groups->Groups[0].Sid) +
                        GetLengthSid(groups->Groups[0].Sid));
                logonSessionSid = reinterpret_cast<PSID>(logonSid.data());
            }
        }
        std::array<EXPLICIT_ACCESSW, 2> accesses{};
        const auto configure = [](EXPLICIT_ACCESSW& access, PSID sid,
            TRUSTEE_TYPE type) {
            access.grfAccessPermissions = FILE_GENERIC_READ | FILE_GENERIC_WRITE;
            access.grfAccessMode = SET_ACCESS;
            access.grfInheritance = NO_INHERITANCE;
            access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
            access.Trustee.TrusteeType = type;
            access.Trustee.ptstrName = reinterpret_cast<LPWSTR>(sid);
        };
        configure(accesses[0], tokenUser->User.Sid, TRUSTEE_IS_USER);
        DWORD accessCount = 1;
        if (logonSessionSid != nullptr) {
            configure(accesses[1], logonSessionSid, TRUSTEE_IS_GROUP);
            accessCount = 2;
        }
        const DWORD aclResult = SetEntriesInAclW(accessCount, accesses.data(),
            nullptr, &acl);
        if (aclResult != ERROR_SUCCESS) {
            errorCode = aclResult;
            return false;
        }
        if (InitializeSecurityDescriptor(&descriptor,
                SECURITY_DESCRIPTOR_REVISION) == FALSE ||
            SetSecurityDescriptorDacl(&descriptor, TRUE, acl, FALSE) == FALSE) {
            errorCode = GetLastError();
            return false;
        }
        attributes.nLength = sizeof(attributes);
        attributes.lpSecurityDescriptor = &descriptor;
        attributes.bInheritHandle = FALSE;
        return true;
    }
};

HANDLE CreateActivationPipe(const std::wstring& pipeName, DWORD& errorCode) {
    LocalActivationPipeSecurity security;
    if (!security.Initialize(errorCode)) return INVALID_HANDLE_VALUE;
    HANDLE pipe = CreateNamedPipeW(pipeName.c_str(), PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT |
            PIPE_REJECT_REMOTE_CLIENTS,
        1, sizeof(ActivationResponse), sizeof(ActivationRequest), 1000,
        &security.attributes);
    if (pipe == INVALID_HANDLE_VALUE) errorCode = GetLastError();
    return pipe;
}

UINT TaskbarCreatedMessage() {
    static const UINT value = RegisterWindowMessageW(L"TaskbarCreated");
    return value;
}

bool RegisterWindowClass() {
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = [](HWND window, UINT message,
        WPARAM wParam, LPARAM lParam) -> LRESULT {
        auto* impl = reinterpret_cast<Win32DesktopBackend::Impl*>(
            GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            impl = static_cast<Win32DesktopBackend::Impl*>(create->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(impl));
        }
        if (impl == nullptr) return DefWindowProcW(window, message, wParam, lParam);

        if (message == TaskbarCreatedMessage()) {
            if (!impl->started || impl->stopping) return 0;
            for (auto& [registration, tray] : impl->trays) {
                (void)registration;
                tray.data.hWnd = impl->window;
                bool restored = false;
                if (tray.added) {
                    restored = Shell_NotifyIconW(NIM_MODIFY, &tray.data) != FALSE;
                    if (!restored) tray.added = false;
                }
                if (!restored &&
                    Shell_NotifyIconW(NIM_ADD, &tray.data) != FALSE) {
                    tray.added = true;
                    tray.data.uVersion = NOTIFYICON_VERSION_4;
                    (void)Shell_NotifyIconW(NIM_SETVERSION, &tray.data);
                    restored = true;
                }
                if (restored) {
                    tray.data.uVersion = NOTIFYICON_VERSION_4;
                    (void)Shell_NotifyIconW(NIM_SETVERSION, &tray.data);
                    impl->EmitHealth(ApplicationCapability::SystemTray,
                        CapabilityStatus::Supported, "TaskbarCreated",
                        {}, std::nullopt, true);
                } else {
                    const DWORD nativeError = GetLastError();
                    std::string reason;
                    const auto environment = ClassifyTrayError(nativeError,
                        reason);
                    impl->EmitHealth(ApplicationCapability::SystemTray,
                        HealthStatusForEnvironment(environment),
                        "TaskbarCreated", reason,
                        static_cast<std::int64_t>(nativeError), true);
                }
            }
            return 0;
        }

        if (message == kTrayCallbackMessage) {
            const auto iconId = static_cast<UINT>(wParam);
            const auto eventCode = static_cast<UINT>(lParam);
            const auto found = std::find_if(impl->trays.begin(),
                impl->trays.end(), [iconId](const auto& value) {
                    return value.second.iconId == iconId;
                });
            if (found == impl->trays.end()) return 0;
            auto& tray = found->second;
            if (eventCode == WM_RBUTTONUP || eventCode == WM_CONTEXTMENU) {
                ShowTrayMenu(*impl, tray);
                return 0;
            }
            if (eventCode == WM_LBUTTONDBLCLK) {
                impl->Emit({DesktopBackendEventType::TrayDoubleClick,
                    tray.registration, 0});
                return 0;
            }
            return 0;
        }

        if (message == WM_HOTKEY) {
            const auto nativeId = static_cast<int>(wParam);
            const auto found = std::find_if(impl->shortcuts.begin(),
                impl->shortcuts.end(), [nativeId](const auto& value) {
                    return value.second.nativeId == nativeId;
                });
            if (found != impl->shortcuts.end()) {
                impl->Emit({DesktopBackendEventType::GlobalShortcutPressed,
                    found->first, 0});
                return 0;
            }
        }
        return DefWindowProcW(window, message, wParam, lParam);
    };
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = kWindowClass;
    if (RegisterClassW(&windowClass) != 0) return true;
    return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

std::wstring Tooltip(const std::string& value) {
    auto converted = ToWide(value);
    if (converted.empty()) converted = L"XiE Application";
    return converted;
}

void SetTooltip(NOTIFYICONDATAW& data, const std::string& value) {
    const auto text = Tooltip(value);
    lstrcpynW(data.szTip, text.c_str(), ARRAYSIZE(data.szTip));
}

ApplicationResult<void> AddTrayIcon(Win32DesktopBackend::Impl& impl,
    TrayNative& tray, const TrayOptions& options) {
    if (!options.iconPath.empty()) {
        return ApplicationResult<void>::Failure(MakeError(
            ApplicationErrorCode::Unsupported, "CreateTray", "system_tray",
            "custom tray icons are not available in the Win32 v1 backend; use the default icon",
            "win32.desktop_services.v1"));
    }
    tray.data = {};
    tray.data.cbSize = sizeof(NOTIFYICONDATAW);
    tray.data.hWnd = impl.window;
    tray.data.uID = tray.iconId;
    tray.data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    tray.data.uCallbackMessage = kTrayCallbackMessage;
    tray.data.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(IDI_APPLICATION));
    SetTooltip(tray.data, options.tooltip);
    if (Shell_NotifyIconW(NIM_ADD, &tray.data) == FALSE) {
        const DWORD nativeError = GetLastError();
        std::string reason;
        const auto environment = ClassifyTrayError(nativeError, reason);
        impl.EmitHealth(ApplicationCapability::SystemTray,
            HealthStatusForEnvironment(environment), "CreateTray", reason,
            static_cast<std::int64_t>(nativeError), false);
        return ApplicationResult<void>::Failure(MakeError(
            ApplicationErrorCode::BackendFailure, "CreateTray", "system_tray",
            "Shell_NotifyIconW(NIM_ADD) failed", "win32.desktop_services.v1",
            nativeError));
    }
    tray.added = true;
    tray.data.uVersion = NOTIFYICON_VERSION_4;
    if (Shell_NotifyIconW(NIM_SETVERSION, &tray.data) == FALSE) {
        const DWORD nativeError = GetLastError();
        (void)Shell_NotifyIconW(NIM_DELETE, &tray.data);
        tray.added = false;
        impl.EmitHealth(ApplicationCapability::SystemTray,
            CapabilityStatus::Degraded, "CreateTray", "tray_version_failed",
            static_cast<std::int64_t>(nativeError), false);
        return ApplicationResult<void>::Failure(MakeError(
            ApplicationErrorCode::BackendFailure, "CreateTray", "system_tray",
            "Shell_NotifyIconW(NIM_SETVERSION) failed",
            "win32.desktop_services.v1", nativeError));
    }
    return ApplicationResult<void>::Success();
}

void ShowTrayMenu(Win32DesktopBackend::Impl& impl, TrayNative& tray) {
    HMENU menu = CreatePopupMenu();
    if (menu == nullptr) return;
    UINT defaultItem = 0;
    for (const auto& item : tray.options.menu) {
        if (item.kind == TrayMenuItemKind::Separator) {
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            continue;
        }
        const auto label = ToWide(item.label);
        UINT flags = MF_STRING;
        if (!item.enabled) flags |= MF_GRAYED;
        if (item.checked) flags |= MF_CHECKED;
        AppendMenuW(menu, flags, static_cast<UINT_PTR>(item.id),
            label.empty() ? L"(unnamed)" : label.c_str());
        if (item.defaultItem) defaultItem = item.id;
    }
    if (defaultItem != 0) SetMenuDefaultItem(menu, defaultItem, FALSE);
    POINT point{};
    (void)GetCursorPos(&point);
    SetForegroundWindow(impl.window);
    const UINT command = TrackPopupMenu(menu,
        TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, 0,
        impl.window, nullptr);
    DestroyMenu(menu);
    if (command != 0) {
        const auto found = std::find_if(tray.options.menu.begin(),
            tray.options.menu.end(), [command](const auto& item) {
                return item.kind == TrayMenuItemKind::Command &&
                    item.id == command && item.enabled;
            });
        if (found != tray.options.menu.end()) {
            impl.Emit({DesktopBackendEventType::TrayCommand,
                tray.registration, found->id});
        }
    }
    PostMessageW(impl.window, WM_NULL, 0, 0);
}

void StartActivationServer(Win32DesktopBackend::Impl& impl,
    const std::wstring& pipeName, std::uint32_t maxActivationBytes) {
    impl.pipeStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (impl.pipeStopEvent == nullptr) {
        impl.activationEndpointError = GetLastError();
        return;
    }
    impl.pipeReadyEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (impl.pipeReadyEvent == nullptr) {
        impl.activationEndpointError = GetLastError();
        CloseHandle(impl.pipeStopEvent);
        impl.pipeStopEvent = nullptr;
        return;
    }
    impl.instancePipeName = pipeName;
    impl.maxActivationBytes = maxActivationBytes;
    impl.activationEndpointReady = false;
    impl.activationEndpointError = ERROR_SUCCESS;
    impl.pipeThread = std::thread([&impl] {
        for (;;) {
            if (WaitForSingleObject(impl.pipeStopEvent, 0) == WAIT_OBJECT_0) break;
            DWORD pipeError = ERROR_SUCCESS;
            HANDLE pipe = CreateActivationPipe(impl.instancePipeName, pipeError);
            if (pipe == INVALID_HANDLE_VALUE) {
                impl.activationEndpointReady = false;
                impl.activationEndpointError = pipeError;
                SetEvent(impl.pipeReadyEvent);
                std::string reason;
                const auto environment = ClassifyActivationError(pipeError,
                    reason);
                impl.EmitHealth(
                    ApplicationCapability::PrimaryInstanceActivation,
                    HealthStatusForEnvironment(environment),
                    "StartActivationEndpoint", reason,
                    static_cast<std::int64_t>(pipeError), false);
                impl.EmitHealth(
                    ApplicationCapability::DesktopIntegration,
                    HealthStatusForEnvironment(environment),
                    "StartActivationEndpoint",
                    "desktop_backend_worker_stopped",
                    static_cast<std::int64_t>(pipeError), false);
                break;
            }
            impl.activationEndpointReady = true;
            SetEvent(impl.pipeReadyEvent);
            BOOL connected = ConnectNamedPipe(pipe, nullptr);
            const DWORD connectError = connected != FALSE
                ? ERROR_SUCCESS : GetLastError();
            if (connected == FALSE && connectError == ERROR_PIPE_CONNECTED) {
                connected = TRUE;
            }
            if (connected != FALSE) {
                ActivationRequest request{};
                DWORD read = 0;
                const BOOL readOk = ReadFile(pipe, &request,
                    sizeof(request), &read, nullptr);
                const bool valid = readOk != FALSE &&
                    read == sizeof(request) &&
                    request.magic == kActivationMagic &&
                    request.version == kActivationVersion &&
                    request.command == kActivatePrimary &&
                    request.payloadBytes == 0 &&
                    request.payloadBytes <= impl.maxActivationBytes;
                ActivationResponse response{};
                response.status = valid ? kActivationAccepted
                                         : kActivationRejected;
                DWORD written = 0;
                (void)WriteFile(pipe, &response, sizeof(response), &written,
                    nullptr);
                if (valid) {
                    impl.Emit({DesktopBackendEventType::PrimaryInstanceActivated,
                        impl.instanceRegistration, 0});
                }
            } else if (WaitForSingleObject(impl.pipeStopEvent, 0) !=
                WAIT_OBJECT_0) {
                std::string reason;
                const auto environment = ClassifyActivationError(connectError,
                    reason);
                impl.EmitHealth(
                    ApplicationCapability::PrimaryInstanceActivation,
                    HealthStatusForEnvironment(environment),
                    "ActivationEndpointConnect", reason,
                    static_cast<std::int64_t>(connectError), false);
            }
            DisconnectNamedPipe(pipe);
            CloseHandle(pipe);
        }
    });
}

void StopActivationServer(Win32DesktopBackend::Impl& impl) noexcept {
    if (impl.pipeStopEvent != nullptr) SetEvent(impl.pipeStopEvent);
    if (impl.pipeThread.joinable()) {
        // The worker uses synchronous ConnectNamedPipe/ReadFile.  Cancel the
        // worker's I/O without closing the pipe from two different threads;
        // the worker remains the sole owner of its temporary pipe handle.
        (void)CancelSynchronousIo(impl.pipeThread.native_handle());
    }
    if (impl.pipeThread.joinable()) impl.pipeThread.join();
    if (impl.pipeStopEvent != nullptr) {
        CloseHandle(impl.pipeStopEvent);
        impl.pipeStopEvent = nullptr;
    }
    if (impl.pipeReadyEvent != nullptr) {
        CloseHandle(impl.pipeReadyEvent);
        impl.pipeReadyEvent = nullptr;
    }
    impl.activationEndpointReady = false;
    impl.activationEndpointError = ERROR_SUCCESS;
    impl.instancePipeName.clear();
    impl.maxActivationBytes = 0;
}

bool WaitForActivationPipe(const std::wstring& pipeName, DWORD timeout,
    DWORD& errorCode) {
    errorCode = ERROR_SUCCESS;
    const ULONGLONG deadline = GetTickCount64() + timeout;
    for (;;) {
        if (WaitNamedPipeW(pipeName.c_str(), 50) != FALSE) return true;
        errorCode = GetLastError();
        if (errorCode != ERROR_FILE_NOT_FOUND &&
            errorCode != ERROR_PATH_NOT_FOUND &&
            errorCode != ERROR_PIPE_BUSY) return false;
        const ULONGLONG now = GetTickCount64();
        if (now >= deadline) return false;
        Sleep(1);
    }
}

bool SendActivation(const std::wstring& pipeName, DWORD timeout,
    DWORD& errorCode) {
    if (!WaitForActivationPipe(pipeName, timeout, errorCode)) return false;
    HANDLE pipe = CreateFileW(pipeName.c_str(), GENERIC_READ | GENERIC_WRITE,
        0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
        errorCode = GetLastError();
        return false;
    }
    ActivationRequest request{};
    DWORD written = 0;
    const BOOL writeOk = WriteFile(pipe, &request, sizeof(request), &written,
        nullptr);
    ActivationResponse response{};
    DWORD read = 0;
    const BOOL readOk = writeOk != FALSE &&
        ReadFile(pipe, &response, sizeof(response), &read, nullptr);
    CloseHandle(pipe);
    if (readOk == FALSE || written != sizeof(request) ||
        read != sizeof(response)) {
        errorCode = GetLastError();
        return false;
    }
    if (response.magic != kActivationMagic ||
        response.version != kActivationVersion ||
        response.status != kActivationAccepted) {
        errorCode = ERROR_INVALID_DATA;
        return false;
    }
    return true;
}

} // namespace

ApplicationResult<void> Win32DesktopBackend::Start(
    const DesktopBackendStartOptions& options) {
    if (!m_Impl) return ApplicationResult<void>::Failure(MakeError(
        ApplicationErrorCode::BackendFailure, "StartDesktopServices", "desktop",
        "Win32 desktop backend state is unavailable", "win32.desktop_services.v1"));
    if (m_Impl->started) return ApplicationResult<void>::Success();
    if (!RegisterWindowClass()) {
        return ApplicationResult<void>::Failure(MakeError(
            ApplicationErrorCode::BackendFailure, "StartDesktopServices", "desktop",
            "desktop message-window class registration failed",
            "win32.desktop_services.v1", GetLastError()));
    }
    m_Impl->callbacks = options;
    m_Impl->stopping = false;
    m_Impl->window = CreateWindowExW(0, kWindowClass,
        L"XiE Desktop Services", WS_POPUP, 0, 0, 1, 1, HWND_MESSAGE,
        nullptr, GetModuleHandleW(nullptr), m_Impl.get());
    if (m_Impl->window == nullptr) {
        m_Impl->callbacks = {};
        return ApplicationResult<void>::Failure(MakeError(
            ApplicationErrorCode::BackendFailure, "StartDesktopServices", "desktop",
            "desktop message window creation failed",
            "win32.desktop_services.v1", GetLastError()));
    }
    m_Impl->started = true;
    return ApplicationResult<void>::Success();
}

ApplicationResult<DesktopRegistrationId> Win32DesktopBackend::CreateTray(
    const TrayOptions& options) {
    if (!m_Impl || !m_Impl->started || m_Impl->window == nullptr) {
        return ApplicationResult<DesktopRegistrationId>::Failure(MakeError(
            ApplicationErrorCode::Unsupported, "CreateTray", "system_tray",
            "desktop backend is not started", "win32.desktop_services.v1"));
    }
    const auto registration = m_Impl->nextRegistration++;
    TrayNative tray;
    tray.registration = registration;
    tray.iconId = static_cast<UINT>(0x1000u +
        static_cast<UINT>(registration & 0x0fffu));
    tray.options = options;
    const auto added = AddTrayIcon(*m_Impl, tray, options);
    if (!added) return ApplicationResult<DesktopRegistrationId>::Failure(added.Error());
    m_Impl->trays.emplace(registration, tray);
    return ApplicationResult<DesktopRegistrationId>::Success(registration);
}

ApplicationResult<void> Win32DesktopBackend::UpdateTray(
    DesktopRegistrationId registration, const std::vector<TrayMenuItem>& menu,
    const std::string& tooltip) {
    if (!m_Impl || !m_Impl->started) return ApplicationResult<void>::Failure(
        MakeError(ApplicationErrorCode::Unsupported, "UpdateTray", "system_tray",
            "desktop backend is not started", "win32.desktop_services.v1"));
    const auto found = m_Impl->trays.find(registration);
    if (found == m_Impl->trays.end()) return ApplicationResult<void>::Failure(
        MakeError(ApplicationErrorCode::LifecycleViolation, "UpdateTray",
            "system_tray", "tray registration is not active",
            "win32.desktop_services.v1"));
    found->second.options.menu = menu;
    if (!tooltip.empty()) found->second.options.tooltip = tooltip;
    SetTooltip(found->second.data, tooltip);
    if (found->second.added &&
        Shell_NotifyIconW(NIM_MODIFY, &found->second.data) == FALSE) {
        const DWORD nativeError = GetLastError();
        std::string reason;
        const auto environment = ClassifyTrayError(nativeError, reason);
        m_Impl->EmitHealth(ApplicationCapability::SystemTray,
            HealthStatusForEnvironment(environment), "UpdateTray", reason,
            static_cast<std::int64_t>(nativeError), false);
        return ApplicationResult<void>::Failure(MakeError(
            ApplicationErrorCode::BackendFailure, "UpdateTray", "system_tray",
            "Shell_NotifyIconW(NIM_MODIFY) failed",
            "win32.desktop_services.v1", nativeError));
    }
    return ApplicationResult<void>::Success();
}

ApplicationResult<void> Win32DesktopBackend::RemoveTray(
    DesktopRegistrationId registration) {
    if (!m_Impl) return ApplicationResult<void>::Success();
    const auto found = m_Impl->trays.find(registration);
    if (found == m_Impl->trays.end()) return ApplicationResult<void>::Success();
    if (found->second.added) {
        (void)Shell_NotifyIconW(NIM_DELETE, &found->second.data);
    }
    m_Impl->trays.erase(found);
    return ApplicationResult<void>::Success();
}

ApplicationResult<DesktopRegistrationId>
Win32DesktopBackend::RegisterGlobalShortcut(const ShortcutOptions& options) {
    if (!m_Impl || !m_Impl->started || m_Impl->window == nullptr) {
        return ApplicationResult<DesktopRegistrationId>::Failure(MakeError(
            ApplicationErrorCode::Unsupported, "RegisterGlobalShortcut",
            "global_shortcut", "desktop backend is not started",
            "win32.desktop_services.v1"));
    }
    UINT modifiers = 0;
    UINT key = 0;
    std::string error;
    if (!ToNativeShortcut(options.key, options.modifiers, modifiers, key, error)) {
        return ApplicationResult<DesktopRegistrationId>::Failure(MakeError(
            ApplicationErrorCode::InvalidOptions, "RegisterGlobalShortcut",
            "global_shortcut", std::move(error),
            "win32.desktop_services.v1"));
    }
    const auto registration = m_Impl->nextRegistration++;
    const int nativeId = static_cast<int>(registration & 0x7fffffff);
    if (RegisterHotKey(m_Impl->window, nativeId, modifiers, key) == FALSE) {
        return ApplicationResult<DesktopRegistrationId>::Failure(MakeError(
            ApplicationErrorCode::Busy, "RegisterGlobalShortcut",
            "global_shortcut", "global shortcut is already registered",
            "win32.desktop_services.v1", GetLastError()));
    }
    m_Impl->shortcuts.emplace(registration,
        ShortcutNative{registration, nativeId});
    return ApplicationResult<DesktopRegistrationId>::Success(registration);
}

ApplicationResult<void> Win32DesktopBackend::UnregisterGlobalShortcut(
    DesktopRegistrationId registration) {
    if (!m_Impl) return ApplicationResult<void>::Success();
    const auto found = m_Impl->shortcuts.find(registration);
    if (found == m_Impl->shortcuts.end()) return ApplicationResult<void>::Success();
    if (m_Impl->window != nullptr) {
        (void)UnregisterHotKey(m_Impl->window, found->second.nativeId);
    }
    m_Impl->shortcuts.erase(found);
    return ApplicationResult<void>::Success();
}

ApplicationResult<SingleInstanceAcquireResult>
Win32DesktopBackend::AcquireSingleInstance(const SingleInstanceOptions& options) {
    SingleInstanceAcquireResult result;
    if (!m_Impl || !m_Impl->started) {
        result.status = SingleInstanceAcquireStatus::BackendFailure;
        result.message = "desktop backend is not started";
        return ApplicationResult<SingleInstanceAcquireResult>::Success(
            std::move(result));
    }
    const auto suffix = HexId(StableId(options.applicationId));
    const auto session = CurrentSessionSuffix();
    const std::wstring mutexName =
        L"Local\\XiE.DesktopServices.Instance." + session + L"." + suffix;
    const std::wstring pipeName =
        L"\\\\.\\pipe\\XiE.DesktopServices.Activate." +
        session + L"." + suffix;
    HANDLE mutex = CreateMutexW(nullptr, TRUE, mutexName.c_str());
    if (mutex == nullptr) {
        result.status = SingleInstanceAcquireStatus::BackendFailure;
        result.message = "single-instance mutex creation failed";
        return ApplicationResult<SingleInstanceAcquireResult>::Success(
            std::move(result));
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        DWORD activationError = ERROR_SUCCESS;
        result.status = SendActivation(pipeName, options.timeoutMilliseconds,
            activationError)
            ? SingleInstanceAcquireStatus::ExistingInstanceActivated
            : SingleInstanceAcquireStatus::AlreadyRunningButUnreachable;
        result.message = result.status ==
            SingleInstanceAcquireStatus::ExistingInstanceActivated
            ? "existing primary instance activated"
            : "existing instance mutex is held but activation endpoint is unreachable " +
                std::to_string(activationError);
        return ApplicationResult<SingleInstanceAcquireResult>::Success(
            std::move(result));
    }

    const auto registration = m_Impl->nextRegistration++;
    m_Impl->instanceRegistration = registration;
    m_Impl->instanceMutex = mutex;
    StartActivationServer(*m_Impl, pipeName, options.maxActivationBytes);
    const DWORD endpointReady = m_Impl->pipeReadyEvent == nullptr
        ? WAIT_FAILED
        : WaitForSingleObject(m_Impl->pipeReadyEvent,
            options.timeoutMilliseconds);
    if (m_Impl->pipeStopEvent == nullptr ||
        !m_Impl->pipeThread.joinable() || endpointReady != WAIT_OBJECT_0 ||
        !m_Impl->activationEndpointReady) {
        const DWORD endpointError = m_Impl->activationEndpointError.load();
        StopActivationServer(*m_Impl);
        if (m_Impl->instanceMutex != nullptr) {
            CloseHandle(m_Impl->instanceMutex);
            m_Impl->instanceMutex = nullptr;
        }
        m_Impl->instanceRegistration = 0;
        result.status = SingleInstanceAcquireStatus::BackendFailure;
        result.message = "single-instance activation endpoint could not start " +
            std::to_string(endpointError);
        return ApplicationResult<SingleInstanceAcquireResult>::Success(
            std::move(result));
    }
    result.status = SingleInstanceAcquireStatus::PrimaryAcquired;
    result.registration = registration;
    result.message = "primary instance acquired";
    return ApplicationResult<SingleInstanceAcquireResult>::Success(
        std::move(result));
}

ApplicationResult<void> Win32DesktopBackend::ReleaseSingleInstance(
    DesktopRegistrationId registration) {
    if (!m_Impl || registration == 0 ||
        registration != m_Impl->instanceRegistration) {
        return ApplicationResult<void>::Success();
    }
    StopActivationServer(*m_Impl);
    if (m_Impl->instanceMutex != nullptr) {
        CloseHandle(m_Impl->instanceMutex);
        m_Impl->instanceMutex = nullptr;
    }
    m_Impl->instanceRegistration = 0;
    return ApplicationResult<void>::Success();
}

void Win32DesktopBackend::Pump() {
    if (!m_Impl || !m_Impl->started) return;
    MSG message{};
    // Only consume messages addressed to the desktop-services host.  The
    // application may share this thread with GLFW/Win32 window hosts; their
    // queue and WM_QUIT state must remain owned by those hosts.
    while (PeekMessageW(&message, m_Impl->window, 0, 0, PM_REMOVE) != FALSE) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

void Win32DesktopBackend::Stop() noexcept {
    if (!m_Impl || !m_Impl->started || m_Impl->stopping) return;
    m_Impl->stopping = true;
    // Prevent a queued TaskbarCreated message from rebuilding an icon while
    // shutdown is removing native objects.
    m_Impl->started = false;
    for (const auto& [registration, shortcut] : m_Impl->shortcuts) {
        (void)registration;
        if (m_Impl->window != nullptr)
            (void)UnregisterHotKey(m_Impl->window, shortcut.nativeId);
    }
    m_Impl->shortcuts.clear();
    for (auto& [registration, tray] : m_Impl->trays) {
        (void)registration;
        if (tray.added) (void)Shell_NotifyIconW(NIM_DELETE, &tray.data);
    }
    m_Impl->trays.clear();
    if (m_Impl->instanceRegistration != 0) {
        (void)ReleaseSingleInstance(m_Impl->instanceRegistration);
    }
    if (m_Impl->window != nullptr) {
        DestroyWindow(m_Impl->window);
        m_Impl->window = nullptr;
    }
    m_Impl->callbacks = {};
}

std::unique_ptr<IDesktopBackend> CreateWin32DesktopBackend() {
    return std::make_unique<Win32DesktopBackend>();
}

} // namespace Engine

#endif // _WIN32
