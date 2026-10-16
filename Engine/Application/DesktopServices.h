#pragma once

#include "Application/ApplicationCapabilities.h"
#include "Application/ApplicationResult.h"
#include "Input/InputEvents.h"
#include "Input/KeyCode.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace Engine {

using DesktopRegistrationId = std::uint64_t;
using DesktopCommandId = std::uint32_t;

enum class TrayMenuItemKind : std::uint8_t {
    Command,
    Separator,
};

struct TrayMenuItem {
    DesktopCommandId id = 0;
    std::string label;
    TrayMenuItemKind kind = TrayMenuItemKind::Command;
    bool enabled = true;
    bool checked = false;
    bool defaultItem = false;
};

struct TrayOptions {
    std::string tooltip = "XiE Application";
    // An empty iconPath asks the backend to use its platform default icon.
    // The path is deliberately a portable string; the backend decides which
    // image formats and resource locations it supports.
    std::string iconPath;
    std::vector<TrayMenuItem> menu;
    std::function<void(DesktopCommandId)> onCommand;
    std::function<void()> onDoubleClick;
};

struct ShortcutOptions {
    InputKeyCode key = InputKeyCode::Unknown;
    InputModifiers modifiers = InputModifiers::None;
    std::function<void()> onPressed;
};

struct SingleInstanceOptions {
    std::string applicationId;
    std::uint32_t maxActivationBytes = 256;
    std::uint32_t timeoutMilliseconds = 500;
    std::function<void()> onPrimaryActivate;
};

// Portable diagnosis of the environment around an optional desktop
// backend.  No shell handles, ACLs, or other platform types cross this
// boundary.
enum class DesktopEnvironmentStatus : std::uint8_t {
    Unknown,
    Ready,
    ExplorerMissing,
    NotificationAreaUnavailable,
    ShellRestarting,
    PermissionDenied,
    SessionIsolated,
    InvalidParameters,
    BackendFailure,
};

inline const char* ToString(DesktopEnvironmentStatus status) noexcept {
    switch (status) {
    case DesktopEnvironmentStatus::Unknown: return "unknown";
    case DesktopEnvironmentStatus::Ready: return "ready";
    case DesktopEnvironmentStatus::ExplorerMissing: return "explorer_missing";
    case DesktopEnvironmentStatus::NotificationAreaUnavailable:
        return "notification_area_unavailable";
    case DesktopEnvironmentStatus::ShellRestarting: return "shell_restarting";
    case DesktopEnvironmentStatus::PermissionDenied: return "permission_denied";
    case DesktopEnvironmentStatus::SessionIsolated: return "session_isolated";
    case DesktopEnvironmentStatus::InvalidParameters: return "invalid_parameters";
    case DesktopEnvironmentStatus::BackendFailure: return "backend_failure";
    }
    return "unknown";
}

enum class SingleInstanceAcquireStatus : std::uint8_t {
    PrimaryAcquired,
    ExistingInstanceActivated,
    AlreadyRunningButUnreachable,
    InvalidApplicationId,
    BackendFailure,
};

enum class DesktopBackendEventType : std::uint8_t {
    TrayCommand,
    TrayDoubleClick,
    GlobalShortcutPressed,
    PrimaryInstanceActivated,
    HealthChanged,
};

struct DesktopBackendEvent {
    DesktopBackendEventType type = DesktopBackendEventType::TrayCommand;
    DesktopRegistrationId registration = 0;
    DesktopCommandId command = 0;
    ApplicationCapability capability = ApplicationCapability::DesktopIntegration;
    CapabilityStatus healthStatus = CapabilityStatus::Supported;
    std::string operation;
    std::string reason;
    std::optional<std::int64_t> nativeError;
    bool recovering = false;
};

struct DesktopBackendCapabilities {
    bool systemTray = false;
    bool globalShortcut = false;
    bool singleInstance = false;
    bool primaryInstanceActivation = false;
    std::string backend;
    DesktopEnvironmentStatus environment = DesktopEnvironmentStatus::Unknown;
    std::string environmentReason;
    std::optional<std::int64_t> environmentNativeError;
};

struct DesktopHealthSnapshot {
    CapabilityHealth desktopIntegration;
    CapabilityHealth systemTray;
    CapabilityHealth globalShortcut;
    CapabilityHealth singleInstance;
    CapabilityHealth primaryInstanceActivation;

    [[nodiscard]] const CapabilityHealth& For(
        ApplicationCapability capability) const noexcept {
        switch (capability) {
        case ApplicationCapability::DesktopIntegration:
            return desktopIntegration;
        case ApplicationCapability::SystemTray:
            return systemTray;
        case ApplicationCapability::GlobalShortcut:
            return globalShortcut;
        case ApplicationCapability::SingleInstance:
            return singleInstance;
        case ApplicationCapability::PrimaryInstanceActivation:
            return primaryInstanceActivation;
        default: break;
        }
        static const CapabilityHealth empty{};
        return empty;
    }
};

struct DesktopBackendStartOptions {
    // Backends may call enqueueEvent from a worker or native callback thread.
    // DesktopServices owns the queue and invokes application callbacks only
    // when Pump() runs on the context owner thread.
    std::function<void(DesktopBackendEvent)> enqueueEvent;
    std::function<void()> wakeOwnerThread;
};

struct SingleInstanceAcquireResult;
namespace Detail {
struct DesktopServicesState;
}

class TraySession final {
public:
    TraySession() = default;
    ~TraySession() noexcept;

    TraySession(const TraySession&) = delete;
    TraySession& operator=(const TraySession&) = delete;
    TraySession(TraySession&& other) noexcept;
    TraySession& operator=(TraySession&& other) noexcept;

    ApplicationResult<void> Update(
        const std::vector<TrayMenuItem>& menu,
        std::string tooltip = {});
    ApplicationResult<void> Remove();
    [[nodiscard]] bool IsActive() const noexcept;
    [[nodiscard]] const std::string& LastError() const noexcept;

    // The detail state type is not part of the application-facing contract;
    // this constructor exists so the owning service can materialize a token.
    TraySession(std::weak_ptr<Detail::DesktopServicesState> state,
        DesktopRegistrationId registration);

private:
    std::weak_ptr<Detail::DesktopServicesState> m_State;
    DesktopRegistrationId m_Registration = 0;
    std::string m_LastError;
};

class ShortcutRegistration final {
public:
    ShortcutRegistration() = default;
    ~ShortcutRegistration() noexcept;

    ShortcutRegistration(const ShortcutRegistration&) = delete;
    ShortcutRegistration& operator=(const ShortcutRegistration&) = delete;
    ShortcutRegistration(ShortcutRegistration&& other) noexcept;
    ShortcutRegistration& operator=(ShortcutRegistration&& other) noexcept;

    ApplicationResult<void> Unregister();
    [[nodiscard]] bool IsRegistered() const noexcept;
    [[nodiscard]] const std::string& LastError() const noexcept;

    ShortcutRegistration(std::weak_ptr<Detail::DesktopServicesState> state,
        DesktopRegistrationId registration);

private:
    std::weak_ptr<Detail::DesktopServicesState> m_State;
    DesktopRegistrationId m_Registration = 0;
    std::string m_LastError;
};

class SingleInstanceSession final {
public:
    SingleInstanceSession() = default;
    ~SingleInstanceSession() noexcept;

    SingleInstanceSession(const SingleInstanceSession&) = delete;
    SingleInstanceSession& operator=(const SingleInstanceSession&) = delete;
    SingleInstanceSession(SingleInstanceSession&& other) noexcept;
    SingleInstanceSession& operator=(SingleInstanceSession&& other) noexcept;

    ApplicationResult<void> Release();
    [[nodiscard]] bool IsPrimary() const noexcept;
    [[nodiscard]] bool IsActive() const noexcept;
    [[nodiscard]] const std::string& LastError() const noexcept;

    SingleInstanceSession(std::weak_ptr<Detail::DesktopServicesState> state,
        DesktopRegistrationId registration);

private:
    std::weak_ptr<Detail::DesktopServicesState> m_State;
    DesktopRegistrationId m_Registration = 0;
    std::string m_LastError;
};

struct SingleInstanceAcquireResult {
    SingleInstanceAcquireStatus status =
        SingleInstanceAcquireStatus::BackendFailure;
    DesktopRegistrationId registration = 0;
    std::optional<SingleInstanceSession> primary;
    std::string message;

    [[nodiscard]] bool IsPrimary() const noexcept {
        return status == SingleInstanceAcquireStatus::PrimaryAcquired &&
            primary.has_value() && primary->IsActive();
    }
    [[nodiscard]] bool IsSecondary() const noexcept {
        return status == SingleInstanceAcquireStatus::ExistingInstanceActivated ||
            status == SingleInstanceAcquireStatus::AlreadyRunningButUnreachable;
    }
};

// Optional, platform-neutral service boundary.  A backend is supplied by a
// platform target; the application-composition target itself remains free of
// shell, IPC, and native window dependencies.
class IDesktopBackend {
public:
    virtual ~IDesktopBackend() = default;

    [[nodiscard]] virtual DesktopBackendCapabilities Probe() const noexcept = 0;
    virtual ApplicationResult<void> Start(
        const DesktopBackendStartOptions& options) = 0;
    virtual ApplicationResult<DesktopRegistrationId> CreateTray(
        const TrayOptions& options) = 0;
    virtual ApplicationResult<void> UpdateTray(
        DesktopRegistrationId registration,
        const std::vector<TrayMenuItem>& menu,
        const std::string& tooltip) = 0;
    virtual ApplicationResult<void> RemoveTray(
        DesktopRegistrationId registration) = 0;
    virtual ApplicationResult<DesktopRegistrationId> RegisterGlobalShortcut(
        const ShortcutOptions& options) = 0;
    virtual ApplicationResult<void> UnregisterGlobalShortcut(
        DesktopRegistrationId registration) = 0;
    virtual ApplicationResult<SingleInstanceAcquireResult>
        AcquireSingleInstance(const SingleInstanceOptions& options) = 0;
    virtual ApplicationResult<void> ReleaseSingleInstance(
        DesktopRegistrationId registration) = 0;
    virtual void Pump() = 0;
    virtual void Stop() noexcept = 0;
};

class DesktopServices final {
public:
    class TrayApi;
    class ShortcutApi;
    class InstanceApi;

    explicit DesktopServices(std::unique_ptr<IDesktopBackend> backend = {});
    ~DesktopServices() noexcept;

    DesktopServices(const DesktopServices&) = delete;
    DesktopServices& operator=(const DesktopServices&) = delete;
    DesktopServices(DesktopServices&&) = delete;
    DesktopServices& operator=(DesktopServices&&) = delete;

    [[nodiscard]] DesktopBackendCapabilities Capabilities() const noexcept;
    [[nodiscard]] DesktopHealthSnapshot Health() const noexcept;
    [[nodiscard]] bool IsStarted() const noexcept;
    [[nodiscard]] const std::string& LastError() const noexcept;

    // Start/Shutdown are idempotent.  With no optional backend, Start still
    // succeeds so ordinary XiE applications can ignore this extension.
    ApplicationResult<void> Start();
    ApplicationResult<void> Pump();
    ApplicationResult<void> Shutdown();

    // The callback is internal coordination expressed as a portable
    // function.  ApplicationContext uses it to wake its existing event loop.
    void SetOwnerWakeCallback(std::function<void()> callback);

    [[nodiscard]] TrayApi Tray() noexcept;
    [[nodiscard]] ShortcutApi Shortcuts() noexcept;
    [[nodiscard]] InstanceApi Instances() noexcept;

private:
    friend class TrayApi;
    friend class ShortcutApi;
    friend class InstanceApi;
    friend class TraySession;
    friend class ShortcutRegistration;
    friend class SingleInstanceSession;

    std::shared_ptr<Detail::DesktopServicesState> m_State;
};

class DesktopServices::TrayApi final {
public:
    ApplicationResult<TraySession> Create(
        const TrayOptions& options = {});

private:
    friend class DesktopServices;
    explicit TrayApi(DesktopServices* owner) : m_Owner(owner) {}
    DesktopServices* m_Owner = nullptr;
};

class DesktopServices::ShortcutApi final {
public:
    ApplicationResult<ShortcutRegistration> Register(
        const ShortcutOptions& options);

private:
    friend class DesktopServices;
    explicit ShortcutApi(DesktopServices* owner) : m_Owner(owner) {}
    DesktopServices* m_Owner = nullptr;
};

class DesktopServices::InstanceApi final {
public:
    ApplicationResult<SingleInstanceAcquireResult> Acquire(
        const SingleInstanceOptions& options);

private:
    friend class DesktopServices;
    explicit InstanceApi(DesktopServices* owner) : m_Owner(owner) {}
    DesktopServices* m_Owner = nullptr;
};

// Implemented only by an optional platform target.  Applications should
// explicitly link that target when they need the platform service; merely
// using ApplicationContext never pulls it in.
#if defined(_WIN32)
std::unique_ptr<IDesktopBackend> CreateWin32DesktopBackend();
#endif

} // namespace Engine
