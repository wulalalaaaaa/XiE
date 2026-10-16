#include "Application/DesktopServices.h"

#include <string>
#include <iostream>
#include <thread>
#include <utility>
#include <vector>

namespace {

class FakeDesktopBackend final : public Engine::IDesktopBackend {
public:
    Engine::DesktopBackendCapabilities Probe() const noexcept override {
        return {true, true, true, true, "fake.desktop_services"};
    }

    Engine::ApplicationResult<void> Start(
        const Engine::DesktopBackendStartOptions& value) override {
        callbacks = value;
        started = true;
        return Engine::ApplicationResult<void>::Success();
    }

    Engine::ApplicationResult<Engine::DesktopRegistrationId> CreateTray(
        const Engine::TrayOptions& value) override {
        if (failTray) return Engine::ApplicationResult<
            Engine::DesktopRegistrationId>::Failure({
                Engine::ApplicationErrorCode::BackendFailure,
                "CreateTray", "system_tray", "fake tray failure", 71,
                "fake.desktop_services"});
        trayOptions = value;
        trayId = nextId++;
        return Engine::ApplicationResult<
            Engine::DesktopRegistrationId>::Success(trayId);
    }

    Engine::ApplicationResult<void> UpdateTray(
        Engine::DesktopRegistrationId id,
        const std::vector<Engine::TrayMenuItem>& menu,
        const std::string& tooltip) override {
        if (failTray) return Engine::ApplicationResult<void>::Failure({
            Engine::ApplicationErrorCode::BackendFailure, "UpdateTray",
            "system_tray", "fake tray failure", 72,
            "fake.desktop_services"});
        if (id != trayId) return Engine::ApplicationResult<void>::Failure({
            Engine::ApplicationErrorCode::LifecycleViolation, "UpdateTray",
            "system_tray", "unknown tray", std::nullopt,
            "fake.desktop_services"});
        trayOptions.menu = menu;
        trayOptions.tooltip = tooltip;
        return Engine::ApplicationResult<void>::Success();
    }

    Engine::ApplicationResult<void> RemoveTray(
        Engine::DesktopRegistrationId id) override {
        if (id == trayId) trayId = 0;
        return Engine::ApplicationResult<void>::Success();
    }

    Engine::ApplicationResult<Engine::DesktopRegistrationId>
    RegisterGlobalShortcut(const Engine::ShortcutOptions& value) override {
        if (failShortcut) return Engine::ApplicationResult<
            Engine::DesktopRegistrationId>::Failure({
                Engine::ApplicationErrorCode::Busy,
                "RegisterGlobalShortcut", "global_shortcut",
                "fake conflict", std::nullopt, "fake.desktop_services"});
        shortcutOptions.push_back(value);
        const auto id = nextId++;
        shortcutIds.push_back(id);
        return Engine::ApplicationResult<
            Engine::DesktopRegistrationId>::Success(id);
    }

    Engine::ApplicationResult<void> UnregisterGlobalShortcut(
        Engine::DesktopRegistrationId id) override {
        for (auto& value : shortcutIds) {
            if (value == id) value = 0;
        }
        return Engine::ApplicationResult<void>::Success();
    }

    Engine::ApplicationResult<Engine::SingleInstanceAcquireResult>
    AcquireSingleInstance(const Engine::SingleInstanceOptions&) override {
        Engine::SingleInstanceAcquireResult result;
        if (secondary) {
            result.status = Engine::SingleInstanceAcquireStatus::
                ExistingInstanceActivated;
            result.message = "fake primary activated";
        } else {
            result.status = Engine::SingleInstanceAcquireStatus::PrimaryAcquired;
            result.registration = nextId++;
            instanceId = result.registration;
        }
        return Engine::ApplicationResult<
            Engine::SingleInstanceAcquireResult>::Success(std::move(result));
    }

    Engine::ApplicationResult<void> ReleaseSingleInstance(
        Engine::DesktopRegistrationId id) override {
        if (id == instanceId) instanceId = 0;
        return Engine::ApplicationResult<void>::Success();
    }

    void Pump() override {}

    void Stop() noexcept override {
        started = false;
        callbacks = {};
    }

    void Emit(Engine::DesktopBackendEvent event) {
        if (callbacks.enqueueEvent) callbacks.enqueueEvent(event);
    }

    Engine::DesktopBackendStartOptions callbacks;
    Engine::TrayOptions trayOptions;
    std::vector<Engine::ShortcutOptions> shortcutOptions;
    std::vector<Engine::DesktopRegistrationId> shortcutIds;
    Engine::DesktopRegistrationId trayId = 0;
    Engine::DesktopRegistrationId instanceId = 0;
    Engine::DesktopRegistrationId nextId = 1;
    bool started = false;
    bool secondary = false;
    bool failShortcut = false;
    bool failTray = false;
};

} // namespace

#define XIE_CHECK(expression) \
    do { if (!(expression)) { std::cerr << "check failed at line " \
        << __LINE__ << '\n'; return 1; } } while (false)

int main() {
    using namespace Engine;
    auto backend = std::make_unique<FakeDesktopBackend>();
    auto* raw = backend.get();
    DesktopServices desktop(std::move(backend));
    XIE_CHECK(desktop.Start());
    XIE_CHECK(desktop.Capabilities().systemTray);
    XIE_CHECK(desktop.Capabilities().globalShortcut);
    const auto startedHealth = desktop.Health();
    XIE_CHECK(startedHealth.systemTray.status == CapabilityStatus::Supported);
    XIE_CHECK(startedHealth.globalShortcut.status == CapabilityStatus::Supported);
    XIE_CHECK(startedHealth.systemTray.revision > 1);

    int trayCommands = 0;
    int trayDoubleClicks = 0;
    auto tray = desktop.Tray().Create({
        "fake", {}, {{1, "Open", TrayMenuItemKind::Command, true, false, true}},
        [&](DesktopCommandId id) { if (id == 1) ++trayCommands; },
        [&] { ++trayDoubleClicks; }});
    XIE_CHECK(tray);
    auto shortcut = desktop.Shortcuts().Register({
        InputKeyCode::S, InputModifiers::Shift, [&] { ++trayCommands; }});
    XIE_CHECK(shortcut);
    int activated = 0;
    auto instance = desktop.Instances().Acquire({
        "fake.application", 256, 500, [&] { ++activated; }});
    XIE_CHECK(instance && instance.Value().IsPrimary());
    const auto beforeDuplicateInstance = desktop.Health();
    auto duplicateInstance = desktop.Instances().Acquire({
        "fake.duplicate", 256, 500, {}});
    XIE_CHECK(!duplicateInstance);
    XIE_CHECK(duplicateInstance.Error().code == ApplicationErrorCode::AlreadyExists);
    const auto afterDuplicateInstance = desktop.Health();
    XIE_CHECK(afterDuplicateInstance.singleInstance.revision ==
        beforeDuplicateInstance.singleInstance.revision);
    XIE_CHECK(afterDuplicateInstance.primaryInstanceActivation.revision ==
        beforeDuplicateInstance.primaryInstanceActivation.revision);

    const auto beforeWrongThread = desktop.Health();
    ApplicationResult<ShortcutRegistration> wrongThread;
    std::thread worker([&] {
        wrongThread = desktop.Shortcuts().Register({
            InputKeyCode::F6, InputModifiers::None, {}});
    });
    worker.join();
    XIE_CHECK(!wrongThread);
    XIE_CHECK(wrongThread.Error().code == ApplicationErrorCode::WrongThread);
    const auto afterWrongThread = desktop.Health();
    XIE_CHECK(afterWrongThread.globalShortcut.revision ==
        beforeWrongThread.globalShortcut.revision);

    const std::vector<InputKeyCode> requiredKeys = {
        InputKeyCode::A, InputKeyCode::B, InputKeyCode::C, InputKeyCode::D,
        InputKeyCode::E, InputKeyCode::F, InputKeyCode::G, InputKeyCode::H,
        InputKeyCode::I, InputKeyCode::J, InputKeyCode::K, InputKeyCode::L,
        InputKeyCode::M, InputKeyCode::N, InputKeyCode::O, InputKeyCode::P,
        InputKeyCode::Q, InputKeyCode::R, InputKeyCode::S, InputKeyCode::T,
        InputKeyCode::U, InputKeyCode::V, InputKeyCode::W, InputKeyCode::X,
        InputKeyCode::Y, InputKeyCode::Z,
        InputKeyCode::Digit0, InputKeyCode::Digit1, InputKeyCode::Digit2,
        InputKeyCode::Digit3, InputKeyCode::Digit4, InputKeyCode::Digit5,
        InputKeyCode::Digit6, InputKeyCode::Digit7, InputKeyCode::Digit8,
        InputKeyCode::Digit9,
        InputKeyCode::F1, InputKeyCode::F2, InputKeyCode::F3,
        InputKeyCode::F4, InputKeyCode::F5, InputKeyCode::F6,
        InputKeyCode::F7, InputKeyCode::F8, InputKeyCode::F9,
        InputKeyCode::F10, InputKeyCode::F11, InputKeyCode::F12,
        InputKeyCode::Escape, InputKeyCode::Tab, InputKeyCode::Space,
        InputKeyCode::Enter, InputKeyCode::Backspace, InputKeyCode::Insert,
        InputKeyCode::Delete, InputKeyCode::Home, InputKeyCode::End,
        InputKeyCode::PageUp, InputKeyCode::PageDown, InputKeyCode::Left,
        InputKeyCode::Right, InputKeyCode::Up, InputKeyCode::Down};
    std::vector<ShortcutRegistration> requiredKeyRegistrations;
    requiredKeyRegistrations.reserve(requiredKeys.size());
    for (const auto key : requiredKeys) {
        auto registered = desktop.Shortcuts().Register({key,
            InputModifiers::Control, {}});
        XIE_CHECK(registered);
        requiredKeyRegistrations.push_back(std::move(registered.Value()));
    }
    const auto beforeUnknownKey = desktop.Health();
    auto unknownKey = desktop.Shortcuts().Register({
        InputKeyCode::Unknown, InputModifiers::None, {}});
    XIE_CHECK(!unknownKey);
    XIE_CHECK(unknownKey.Error().code == ApplicationErrorCode::InvalidOptions);
    const auto afterUnknownKey = desktop.Health();
    XIE_CHECK(afterUnknownKey.globalShortcut.revision ==
        beforeUnknownKey.globalShortcut.revision);
    raw->Emit({DesktopBackendEventType::TrayCommand, raw->trayId, 1});
    raw->Emit({DesktopBackendEventType::TrayDoubleClick, raw->trayId, 0});
    raw->Emit({DesktopBackendEventType::GlobalShortcutPressed,
        raw->shortcutIds.front(), 0});
    raw->Emit({DesktopBackendEventType::PrimaryInstanceActivated,
        raw->instanceId, 0});
    XIE_CHECK(desktop.Pump());
    XIE_CHECK(trayCommands == 2 && trayDoubleClicks == 1 && activated == 1);

    XIE_CHECK(shortcut.Value().Unregister());
    auto reusedShortcut = desktop.Shortcuts().Register({
        InputKeyCode::S, InputModifiers::Shift, {}});
    XIE_CHECK(reusedShortcut);
    XIE_CHECK(reusedShortcut.Value().Unregister());

    const auto beforeInvalid = desktop.Health();
    auto invalidTray = desktop.Tray().Create({
        "duplicate", {},
        {{9, "same", TrayMenuItemKind::Command},
         {9, "same again", TrayMenuItemKind::Command}}});
    XIE_CHECK(!invalidTray);
    XIE_CHECK(invalidTray.Error().code == ApplicationErrorCode::InvalidOptions);
    const auto afterInvalid = desktop.Health();
    XIE_CHECK(afterInvalid.systemTray.revision ==
        beforeInvalid.systemTray.revision);
    XIE_CHECK(afterInvalid.systemTray.consecutiveFailures ==
        beforeInvalid.systemTray.consecutiveFailures);

    raw->failShortcut = true;
    const auto beforeBusy = desktop.Health();
    auto busyShortcut = desktop.Shortcuts().Register({
        InputKeyCode::F5, InputModifiers::Control, {}});
    XIE_CHECK(!busyShortcut);
    XIE_CHECK(busyShortcut.Error().code == ApplicationErrorCode::Busy);
    const auto afterBusy = desktop.Health();
    XIE_CHECK(afterBusy.globalShortcut.revision ==
        beforeBusy.globalShortcut.revision);
    XIE_CHECK(afterBusy.globalShortcut.consecutiveFailures ==
        beforeBusy.globalShortcut.consecutiveFailures);
    raw->failShortcut = false;

    raw->failTray = true;
    auto failedTray = desktop.Tray().Create({
        "failing", {}, {{8, "Failure", TrayMenuItemKind::Command}}});
    XIE_CHECK(!failedTray);
    const auto trayFailureHealth = desktop.Health().systemTray;
    XIE_CHECK(trayFailureHealth.status == CapabilityStatus::Degraded);
    XIE_CHECK(trayFailureHealth.lastFailedOperation == "CreateTray");
    XIE_CHECK(trayFailureHealth.lastNativeError.has_value());
    XIE_CHECK(trayFailureHealth.consecutiveFailures >= 1);
    raw->failTray = false;
    auto recoveredTray = desktop.Tray().Create({
        "recovered", {}, {{8, "Recovered", TrayMenuItemKind::Command}}});
    XIE_CHECK(recoveredTray);
    const auto recoveredHealth = desktop.Health().systemTray;
    XIE_CHECK(recoveredHealth.status == CapabilityStatus::Supported);
    XIE_CHECK(recoveredHealth.lastFailedOperation == "CreateTray");
    XIE_CHECK(recoveredHealth.lastNativeError.has_value());
    XIE_CHECK(recoveredHealth.consecutiveFailures == 0);

    raw->Emit({DesktopBackendEventType::HealthChanged, 0, 0,
        ApplicationCapability::SystemTray, CapabilityStatus::Degraded,
        "TaskbarCreated", "tray_rebuild_failed", 73, true});
    XIE_CHECK(desktop.Pump());
    const auto rebuildFailure = desktop.Health().systemTray;
    XIE_CHECK(rebuildFailure.status == CapabilityStatus::Degraded);
    XIE_CHECK(rebuildFailure.reason == "tray_rebuild_failed");
    XIE_CHECK(rebuildFailure.recovering);
    raw->Emit({DesktopBackendEventType::HealthChanged, 0, 0,
        ApplicationCapability::SystemTray, CapabilityStatus::Supported,
        "TaskbarCreated", {}, std::nullopt, true});
    XIE_CHECK(desktop.Pump());
    const auto rebuildRecovery = desktop.Health().systemTray;
    XIE_CHECK(rebuildRecovery.status == CapabilityStatus::Supported);
    XIE_CHECK(rebuildRecovery.lastFailedOperation == "TaskbarCreated");
    XIE_CHECK(rebuildRecovery.lastNativeError == 73);

    XIE_CHECK(recoveredTray.Value().Update({
        {1, "Updated", TrayMenuItemKind::Command, true, true, true}}));
    XIE_CHECK(recoveredTray.Value().Remove());
    XIE_CHECK(recoveredTray.Value().Remove());
    XIE_CHECK(shortcut.Value().Unregister());
    XIE_CHECK(shortcut.Value().Unregister());
    XIE_CHECK(instance.Value().primary->Release());
    XIE_CHECK(instance.Value().primary->Release());

    const auto commandsBeforeEarlyDestroy = trayCommands;
    DesktopRegistrationId shortLivedTray = 0;
    {
        auto temporary = desktop.Tray().Create({
            "temporary", {},
            {{2, "Temporary", TrayMenuItemKind::Command, true, false, false}},
            [&](DesktopCommandId id) { if (id == 2) ++trayCommands; }, {}});
        XIE_CHECK(temporary);
        shortLivedTray = raw->trayId;
    }
    raw->Emit({DesktopBackendEventType::TrayCommand, shortLivedTray, 2});
    XIE_CHECK(desktop.Pump());
    XIE_CHECK(trayCommands == commandsBeforeEarlyDestroy);

    const auto commandsBeforeShortcutDestroy = trayCommands;
    DesktopRegistrationId shortLivedShortcut = 0;
    {
        auto temporary = desktop.Shortcuts().Register({
            InputKeyCode::W, InputModifiers::Alt,
            [&] { ++trayCommands; }});
        XIE_CHECK(temporary);
        shortLivedShortcut = raw->shortcutIds.back();
    }
    raw->Emit({DesktopBackendEventType::GlobalShortcutPressed,
        shortLivedShortcut, 0});
    XIE_CHECK(desktop.Pump());
    XIE_CHECK(trayCommands == commandsBeforeShortcutDestroy);

    XIE_CHECK(desktop.Shutdown());
    XIE_CHECK(desktop.Shutdown());

    auto secondaryBackend = std::make_unique<FakeDesktopBackend>();
    secondaryBackend->secondary = true;
    DesktopServices secondaryDesktop(std::move(secondaryBackend));
    XIE_CHECK(secondaryDesktop.Start());
    auto secondary = secondaryDesktop.Instances().Acquire({"fake.secondary"});
    XIE_CHECK(secondary && secondary.Value().IsSecondary());
    XIE_CHECK(!secondary.Value().primary.has_value());
    XIE_CHECK(secondaryDesktop.Shutdown());
    return 0;
}

#undef XIE_CHECK
