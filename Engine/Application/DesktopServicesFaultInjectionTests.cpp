#include "Application/DesktopServices.h"

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

class FaultBackend final : public Engine::IDesktopBackend {
public:
    Engine::DesktopBackendCapabilities Probe() const noexcept override {
        return {true, true, true, true, "fault.desktop_services"};
    }

    Engine::ApplicationResult<void> Start(
        const Engine::DesktopBackendStartOptions& value) override {
        callbacks = value;
        if (failStart) return Fail<void>("StartDesktopServices",
            "desktop", "backend start failed", 81);
        started = true;
        return Engine::ApplicationResult<void>::Success();
    }

    Engine::ApplicationResult<Engine::DesktopRegistrationId> CreateTray(
        const Engine::TrayOptions& value) override {
        if (failTrayCreate) return Fail<Engine::DesktopRegistrationId>(
            "CreateTray", "system_tray", "tray create failed", 82);
        trayOptions = value;
        trayId = nextId++;
        return Engine::ApplicationResult<
            Engine::DesktopRegistrationId>::Success(trayId);
    }

    Engine::ApplicationResult<void> UpdateTray(
        Engine::DesktopRegistrationId id,
        const std::vector<Engine::TrayMenuItem>& menu,
        const std::string& tooltip) override {
        if (failTrayUpdate) return Fail<void>("UpdateTray", "system_tray",
            "tray update failed", 83);
        if (id != trayId) return Fail<void>("UpdateTray", "system_tray",
            "unknown tray", 0, Engine::ApplicationErrorCode::LifecycleViolation);
        trayOptions.menu = menu;
        trayOptions.tooltip = tooltip;
        return Engine::ApplicationResult<void>::Success();
    }

    Engine::ApplicationResult<void> RemoveTray(
        Engine::DesktopRegistrationId id) override {
        if (failTrayRemove) return Fail<void>("RemoveTray", "system_tray",
            "tray remove failed", 84);
        if (id == trayId) trayId = 0;
        return Engine::ApplicationResult<void>::Success();
    }

    Engine::ApplicationResult<Engine::DesktopRegistrationId>
    RegisterGlobalShortcut(const Engine::ShortcutOptions& value) override {
        if (failShortcutRegister) return Fail<Engine::DesktopRegistrationId>(
            "RegisterGlobalShortcut", "global_shortcut",
            "shortcut registration failed", 85);
        shortcutOptions.push_back(value);
        const auto id = nextId++;
        shortcutIds.push_back(id);
        return Engine::ApplicationResult<
            Engine::DesktopRegistrationId>::Success(id);
    }

    Engine::ApplicationResult<void> UnregisterGlobalShortcut(
        Engine::DesktopRegistrationId id) override {
        if (failShortcutUnregister) return Fail<void>(
            "UnregisterGlobalShortcut", "global_shortcut",
            "shortcut unregister failed", 86);
        for (auto& value : shortcutIds) {
            if (value == id) value = 0;
        }
        return Engine::ApplicationResult<void>::Success();
    }

    Engine::ApplicationResult<Engine::SingleInstanceAcquireResult>
    AcquireSingleInstance(const Engine::SingleInstanceOptions&) override {
        if (failInstanceAcquire) return Fail<
            Engine::SingleInstanceAcquireResult>("AcquireSingleInstance",
                "single_instance", "instance acquire failed", 87);
        Engine::SingleInstanceAcquireResult result;
        result.status = Engine::SingleInstanceAcquireStatus::PrimaryAcquired;
        result.registration = nextId++;
        instanceId = result.registration;
        return Engine::ApplicationResult<
            Engine::SingleInstanceAcquireResult>::Success(std::move(result));
    }

    Engine::ApplicationResult<void> ReleaseSingleInstance(
        Engine::DesktopRegistrationId id) override {
        if (failInstanceRelease) return Fail<void>(
            "ReleaseSingleInstance", "single_instance",
            "instance release failed", 88);
        if (id == instanceId) instanceId = 0;
        return Engine::ApplicationResult<void>::Success();
    }

    void Pump() override {}

    void Stop() noexcept override {
        ++stopCount;
        started = false;
        callbacks = {};
    }

    void Emit(Engine::DesktopBackendEvent event) {
        if (callbacks.enqueueEvent) callbacks.enqueueEvent(std::move(event));
    }

    template <typename T>
    static Engine::ApplicationResult<T> Fail(const char* operation,
        const char* capability, const char* message, std::int64_t nativeCode,
        Engine::ApplicationErrorCode code =
            Engine::ApplicationErrorCode::BackendFailure) {
        return Engine::ApplicationResult<T>::Failure({
            code, operation, capability, message,
            nativeCode == 0 ? std::optional<std::int64_t>{}
                            : std::optional<std::int64_t>{nativeCode},
            "fault.desktop_services"});
    }

    Engine::DesktopBackendStartOptions callbacks;
    Engine::TrayOptions trayOptions;
    std::vector<Engine::ShortcutOptions> shortcutOptions;
    std::vector<Engine::DesktopRegistrationId> shortcutIds;
    Engine::DesktopRegistrationId trayId = 0;
    Engine::DesktopRegistrationId instanceId = 0;
    Engine::DesktopRegistrationId nextId = 1;
    std::uint32_t stopCount = 0;
    bool started = false;
    bool failStart = false;
    bool failTrayCreate = false;
    bool failTrayUpdate = false;
    bool failTrayRemove = false;
    bool failShortcutRegister = false;
    bool failShortcutUnregister = false;
    bool failInstanceAcquire = false;
    bool failInstanceRelease = false;
};

} // namespace

#define XIE_CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::cerr << "check failed at line " << __LINE__ << '\n'; \
            return 1; \
        } \
    } while (false)

int main() {
    using namespace Engine;

    auto backend = std::make_unique<FaultBackend>();
    auto* raw = backend.get();
    DesktopServices desktop(std::move(backend));

    raw->failStart = true;
    const auto startFailure = desktop.Start();
    XIE_CHECK(!startFailure);
    XIE_CHECK(startFailure.Error().code == ApplicationErrorCode::BackendFailure);
    XIE_CHECK(desktop.Health().systemTray.status ==
        CapabilityStatus::Unavailable);
    XIE_CHECK(desktop.Health().systemTray.reason == "backend_start_failed");
    raw->failStart = false;
    XIE_CHECK(desktop.Start());
    XIE_CHECK(desktop.Start());
    XIE_CHECK(desktop.Health().systemTray.status == CapabilityStatus::Supported);

    auto tray = desktop.Tray().Create({
        "fault", {}, {{1, "Open", TrayMenuItemKind::Command}}});
    XIE_CHECK(tray);
    raw->failTrayUpdate = true;
    const auto updateFailure = tray.Value().Update({
        {1, "Updated", TrayMenuItemKind::Command, true, true, true}});
    XIE_CHECK(!updateFailure);
    XIE_CHECK(updateFailure.Error().code == ApplicationErrorCode::BackendFailure);
    const auto failedTrayHealth = desktop.Health().systemTray;
    XIE_CHECK(failedTrayHealth.status == CapabilityStatus::Degraded);
    XIE_CHECK(failedTrayHealth.reason == "backend_operation_failed");
    XIE_CHECK(failedTrayHealth.lastFailedOperation == "UpdateTray");
    XIE_CHECK(failedTrayHealth.lastNativeError == 83);
    XIE_CHECK(failedTrayHealth.consecutiveFailures == 1);
    raw->failTrayUpdate = false;
    XIE_CHECK(tray.Value().Update({
        {1, "Recovered", TrayMenuItemKind::Command, true, false, true}}));
    XIE_CHECK(desktop.Health().systemTray.status == CapabilityStatus::Supported);
    XIE_CHECK(desktop.Health().systemTray.consecutiveFailures == 0);

    raw->failShortcutRegister = true;
    const auto shortcutFailure = desktop.Shortcuts().Register({
        InputKeyCode::S, InputModifiers::Shift, {}});
    XIE_CHECK(!shortcutFailure);
    XIE_CHECK(shortcutFailure.Error().code == ApplicationErrorCode::BackendFailure);
    XIE_CHECK(desktop.Health().globalShortcut.status == CapabilityStatus::Degraded);
    XIE_CHECK(desktop.Health().globalShortcut.lastFailedOperation ==
        "RegisterGlobalShortcut");
    raw->failShortcutRegister = false;
    auto shortcut = desktop.Shortcuts().Register({
        InputKeyCode::S, InputModifiers::Shift, {}});
    XIE_CHECK(shortcut);
    raw->failShortcutUnregister = true;
    const auto unregisterFailure = shortcut.Value().Unregister();
    XIE_CHECK(!unregisterFailure);
    XIE_CHECK(unregisterFailure.Error().code == ApplicationErrorCode::BackendFailure);
    raw->failShortcutUnregister = false;
    XIE_CHECK(shortcut.Value().Unregister());

    raw->failInstanceAcquire = true;
    const auto instanceFailure = desktop.Instances().Acquire({
        "fault.instance"});
    XIE_CHECK(!instanceFailure);
    XIE_CHECK(instanceFailure.Error().code == ApplicationErrorCode::BackendFailure);
    XIE_CHECK(desktop.Health().singleInstance.status ==
        CapabilityStatus::Degraded);
    raw->failInstanceAcquire = false;

    raw->Emit({DesktopBackendEventType::HealthChanged, 0, 0,
        ApplicationCapability::PrimaryInstanceActivation,
        CapabilityStatus::Degraded, "WorkerStopped",
        "activation_endpoint_unreachable", 89, true});
    XIE_CHECK(desktop.Pump());
    const auto endpointFailure = desktop.Health().primaryInstanceActivation;
    XIE_CHECK(endpointFailure.status == CapabilityStatus::Degraded);
    XIE_CHECK(endpointFailure.reason == "activation_endpoint_unreachable");
    XIE_CHECK(endpointFailure.recovering);
    raw->Emit({DesktopBackendEventType::HealthChanged, 0, 0,
        ApplicationCapability::PrimaryInstanceActivation,
        CapabilityStatus::Supported, "WorkerRestarted", {}, std::nullopt,
        false});
    XIE_CHECK(desktop.Pump());
    const auto endpointRecovery = desktop.Health().primaryInstanceActivation;
    XIE_CHECK(endpointRecovery.status == CapabilityStatus::Supported);
    XIE_CHECK(endpointRecovery.lastFailedOperation == "WorkerStopped");
    XIE_CHECK(endpointRecovery.lastNativeError == 89);
    XIE_CHECK(!endpointRecovery.recovering);

    auto instance = desktop.Instances().Acquire({"fault.instance"});
    XIE_CHECK(instance && instance.Value().IsPrimary());
    XIE_CHECK(instance.Value().primary.has_value());
    raw->Emit({DesktopBackendEventType::HealthChanged, 0, 0,
        ApplicationCapability::SystemTray, CapabilityStatus::Unavailable,
        "TaskbarCreated", "shell_temporarily_unavailable", 90, true});
    XIE_CHECK(desktop.Pump());
    XIE_CHECK(desktop.Health().systemTray.status == CapabilityStatus::Unavailable);
    XIE_CHECK(desktop.Health().systemTray.recovering);
    raw->Emit({DesktopBackendEventType::HealthChanged, 0, 0,
        ApplicationCapability::SystemTray, CapabilityStatus::Supported,
        "TaskbarCreated", {}, std::nullopt, true});
    XIE_CHECK(desktop.Pump());
    XIE_CHECK(desktop.Health().systemTray.status == CapabilityStatus::Supported);
    XIE_CHECK(desktop.Health().systemTray.lastFailedOperation == "TaskbarCreated");

    raw->failInstanceRelease = true;
    const auto releaseFailure = instance.Value().primary->Release();
    XIE_CHECK(!releaseFailure);
    raw->failInstanceRelease = false;
    XIE_CHECK(instance.Value().primary->Release());
    XIE_CHECK(tray.Value().Remove());

    XIE_CHECK(desktop.Shutdown());
    XIE_CHECK(desktop.Shutdown());
    XIE_CHECK(raw->stopCount == 1);
    const auto restartAfterShutdown = desktop.Start();
    XIE_CHECK(!restartAfterShutdown);
    XIE_CHECK(restartAfterShutdown.Error().code ==
        ApplicationErrorCode::ShuttingDown);
    raw->Emit({DesktopBackendEventType::HealthChanged, 0, 0,
        ApplicationCapability::SystemTray, CapabilityStatus::Supported,
        "LateEvent", {}, std::nullopt, false});
    XIE_CHECK(desktop.Pump());
    return 0;
}

#undef XIE_CHECK
