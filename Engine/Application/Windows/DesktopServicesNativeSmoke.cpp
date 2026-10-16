#include "Application/ApplicationContext.h"

#ifdef _WIN32

#include "Application/DesktopServices.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <string>
#include <thread>
#include <utility>

namespace {

using namespace Engine;

constexpr int kSuccess = 0;
constexpr int kContextFailure = 10;
constexpr int kDesktopStartFailure = 11;
constexpr int kTrayFailure = 12;
constexpr int kShortcutFailure = 13;
constexpr int kInstanceFailure = 14;
constexpr int kPumpFailure = 15;
constexpr int kShutdownFailure = 16;
constexpr int kEnvironmentBlocked = 20;

std::string JsonEscape(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const unsigned char character : value) {
        switch (character) {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (character < 0x20u) result += '?';
            else result += static_cast<char>(character);
            break;
        }
    }
    return result;
}

void Log(std::string_view phase, std::string_view result,
    std::string_view detail = {}, std::string_view environment = {},
    std::string_view reason = {}, int code = 0) {
    std::cout << "{\"tool\":\"xie_desktop_native_smoke\",\"phase\":\""
              << JsonEscape(phase) << "\",\"result\":\""
              << JsonEscape(result) << "\",\"code\":" << code
              << ",\"detail\":\"" << JsonEscape(detail)
              << "\",\"environment\":\"" << JsonEscape(environment)
              << "\",\"reason\":\"" << JsonEscape(reason) << "\"}\n";
    std::cout.flush();
}

std::string ErrorText(const ApplicationError& error) {
    std::string result = ToString(error.code);
    if (!error.operation.empty()) {
        result += " operation=" + error.operation;
    }
    if (!error.message.empty()) result += " message=" + error.message;
    if (error.nativeCode) {
        result += " native=" + std::to_string(*error.nativeCode);
    }
    return result;
}

int WaitMilliseconds() {
    const char* value = std::getenv("XIE_DESKTOP_SMOKE_WAIT_MS");
    if (value == nullptr || *value == '\0') return 250;
    try {
        std::size_t consumed = 0;
        const auto parsed = std::stoul(value, &consumed);
        if (consumed != std::string(value).size()) return 250;
        return static_cast<int>(parsed > 600000ul ? 600000ul : parsed);
    } catch (...) {
        return 250;
    }
}

InputModifiers ShiftAlt() {
    return static_cast<InputModifiers>(
        static_cast<std::uint8_t>(InputModifiers::Shift) |
        static_cast<std::uint8_t>(InputModifiers::Alt));
}

} // namespace

int main() {
    const char* enabled = std::getenv("XIE_RUN_DESKTOP_SMOKE");
    if (enabled == nullptr || std::string(enabled) != "1") {
        Log("preflight", "not_run",
            "set XIE_RUN_DESKTOP_SMOKE=1 to enable real desktop mutation",
            {}, {}, kSuccess);
        return kSuccess;
    }

    ApplicationContextOptions contextOptions;
    contextOptions.applicationName = "XiE Desktop Services Smoke";
    contextOptions.initializeBackends = false;
    contextOptions.desktopBackend = CreateWin32DesktopBackend();
    auto contextResult = ApplicationContext::Create(
        std::move(contextOptions));
    if (!contextResult) {
        Log("context", "failed", ErrorText(contextResult.Error()), {}, {},
            kContextFailure);
        return kContextFailure;
    }
    auto context = std::move(contextResult.Value());
    const auto environment = context->Desktop().Capabilities();
    const std::string environmentName = ToString(environment.environment);
    Log("context", "passed", "ApplicationContext created",
        environmentName, environment.environmentReason, kSuccess);
    Log("environment",
        environment.environment == DesktopEnvironmentStatus::Ready
            ? "ready" : "blocked",
        environment.environment == DesktopEnvironmentStatus::Ready
            ? "Explorer and notification area detected"
            : "Environment Blocked / Not Tested: Explorer or its notification area is unavailable",
        environmentName, environment.environmentReason,
        environment.environment == DesktopEnvironmentStatus::Ready
            ? kSuccess : kEnvironmentBlocked);
    if (environment.environment != DesktopEnvironmentStatus::Ready) {
        const auto shutdown = context->Shutdown();
        Log("shutdown", shutdown ? "passed" : "failed",
            shutdown ? "blocked preflight context cleaned"
                     : ErrorText(shutdown.Error()), environmentName,
            shutdown ? "" : "shutdown_after_environment_blocked",
            shutdown ? kSuccess : kShutdownFailure);
        return shutdown ? kEnvironmentBlocked : kShutdownFailure;
    }
    if (const auto started = context->Desktop().Start(); !started) {
        Log("desktop_start", "failed", ErrorText(started.Error()),
            environmentName, environment.environmentReason,
            kDesktopStartFailure);
        (void)context->Shutdown();
        return kDesktopStartFailure;
    }
    Log("desktop_start", "passed", "Win32 desktop backend started",
        environmentName, {}, kSuccess);

    std::uint32_t trayCommandCount = 0;
    std::uint32_t trayDoubleClickCount = 0;
    TrayOptions trayOptions;
    trayOptions.tooltip = "XiE Desktop Services Smoke";
    trayOptions.menu = {{1, "Smoke command", TrayMenuItemKind::Command,
        true, false, true}, {0, {}, TrayMenuItemKind::Separator},
        {2, "Exit smoke", TrayMenuItemKind::Command}};
    trayOptions.onCommand = [&](DesktopCommandId) { ++trayCommandCount; };
    trayOptions.onDoubleClick = [&] { ++trayDoubleClickCount; };
    auto tray = context->Desktop().Tray().Create(trayOptions);
    if (!tray) {
        Log("tray_create", "failed", ErrorText(tray.Error()),
            environmentName, context->Desktop().Health().systemTray.reason,
            kTrayFailure);
        (void)context->Shutdown();
        return kTrayFailure;
    }
    Log("tray_create", "passed", "tray icon added",
        environmentName, {}, kSuccess);

    const std::vector<TrayMenuItem> updatedMenu = {
        {1, "Smoke command (updated)", TrayMenuItemKind::Command,
            true, true, true},
        {0, {}, TrayMenuItemKind::Separator},
        {2, "Exit smoke", TrayMenuItemKind::Command, true, false, false}};
    if (const auto updated = tray.Value().Update(updatedMenu,
            "XiE Desktop Services Smoke (updated)"); !updated) {
        Log("tray_update", "failed", ErrorText(updated.Error()),
            environmentName, context->Desktop().Health().systemTray.reason,
            kTrayFailure);
        (void)context->Shutdown();
        return kTrayFailure;
    }
    Log("tray_update", "passed", "tooltip and menu updated",
        environmentName, {}, kSuccess);

    std::uint32_t shortcutPressedCount = 0;
    ShortcutOptions shortcutOptions;
    shortcutOptions.key = InputKeyCode::S;
    shortcutOptions.modifiers = ShiftAlt();
    shortcutOptions.onPressed = [&] { ++shortcutPressedCount; };
    auto shortcut = context->Desktop().Shortcuts().Register(
        shortcutOptions);
    if (!shortcut) {
        Log("shortcut_register", "failed", ErrorText(shortcut.Error()),
            environmentName, context->Desktop().Health().globalShortcut.reason,
            kShortcutFailure);
        (void)context->Shutdown();
        return kShortcutFailure;
    }
    Log("shortcut_register", "passed", "Shift+Alt+S registered",
        environmentName, {}, kSuccess);

    // A second registration of the same chord must be reported as a
    // structured conflict.  After the first token is released the same chord
    // is registered again to exercise replacement/re-registration.
    auto conflict = context->Desktop().Shortcuts().Register(shortcutOptions);
    if (conflict) {
        (void)conflict.Value().Unregister();
        Log("shortcut_conflict", "failed",
            "duplicate chord was accepted instead of reporting a conflict",
            environmentName, "hotkey_conflict_not_reported",
            kShortcutFailure);
        (void)context->Shutdown();
        return kShortcutFailure;
    }
    Log("shortcut_conflict", "passed", ErrorText(conflict.Error()),
        environmentName, "duplicate chord rejected", kSuccess);
    if (const auto unregistered = shortcut.Value().Unregister();
        !unregistered) {
        Log("shortcut_unregister", "failed", ErrorText(unregistered.Error()),
            environmentName, "hotkey_unregister_failed", kShortcutFailure);
        (void)context->Shutdown();
        return kShortcutFailure;
    }
    Log("shortcut_unregister", "passed", "first registration removed",
        environmentName, {}, kSuccess);
    auto replacement = context->Desktop().Shortcuts().Register(shortcutOptions);
    if (!replacement) {
        Log("shortcut_reregister", "failed", ErrorText(replacement.Error()),
            environmentName, "hotkey_reregister_failed", kShortcutFailure);
        (void)context->Shutdown();
        return kShortcutFailure;
    }
    Log("shortcut_reregister", "passed", "same chord registered again",
        environmentName, {}, kSuccess);

    std::uint32_t primaryActivationCount = 0;
    SingleInstanceOptions instanceOptions;
    instanceOptions.applicationId = "xie.desktop.native.smoke";
    instanceOptions.onPrimaryActivate = [&] { ++primaryActivationCount; };
    auto instance = context->Desktop().Instances().Acquire(instanceOptions);
    if (!instance || instance.Value().status !=
        SingleInstanceAcquireStatus::PrimaryAcquired) {
        if (!instance) {
            Log("single_instance_primary", "failed",
                ErrorText(instance.Error()), environmentName,
                context->Desktop().Health().singleInstance.reason,
                kInstanceFailure);
        } else {
            Log("single_instance_primary", "failed",
                "status was not PrimaryAcquired", environmentName,
                "unexpected_single_instance_status", kInstanceFailure);
        }
        (void)context->Shutdown();
        return kInstanceFailure;
    }
    Log("single_instance_primary", "passed",
        "primary mutex and activation endpoint acquired",
        environmentName, {}, kSuccess);

    const int waitMilliseconds = WaitMilliseconds();
    if (waitMilliseconds > 1000) {
        Log("manual_window", "available",
            "while this process is running, click the tray icon, double-click it, and press Shift+Alt+S; no Explorer restart is performed",
            environmentName, {}, kSuccess);
    }
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(waitMilliseconds);
    while (std::chrono::steady_clock::now() < deadline) {
        if (const auto pumped = context->PumpDesktopEvents(); !pumped) {
            Log("pump", "failed", ErrorText(pumped.Error()),
                environmentName, "desktop_pump_failed", kPumpFailure);
            (void)context->Shutdown();
            return kPumpFailure;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    const auto health = context->Desktop().Health();
    Log("health", "observed",
        std::string("tray=") + ToString(health.systemTray.status) +
            ",shortcut=" + ToString(health.globalShortcut.status) +
            ",instance=" + ToString(health.singleInstance.status),
        environmentName, {}, kSuccess);
    Log("pump", "passed",
        std::string("tray_commands=") + std::to_string(trayCommandCount) +
            ",tray_double_clicks=" + std::to_string(trayDoubleClickCount) +
            ",shortcut_presses=" + std::to_string(shortcutPressedCount) +
            ",primary_activations=" +
                std::to_string(primaryActivationCount),
        environmentName, {}, kSuccess);
    if (const auto removed = tray.Value().Remove(); !removed) {
        Log("tray_remove", "failed", ErrorText(removed.Error()),
            environmentName, "tray_remove_failed", kTrayFailure);
        (void)context->Shutdown();
        return kTrayFailure;
    }
    Log("tray_remove", "passed", "tray icon removed explicitly",
        environmentName, {}, kSuccess);
    if (const auto unregistered = replacement.Value().Unregister();
        !unregistered) {
        Log("shortcut_unregister", "failed",
            ErrorText(unregistered.Error()), environmentName,
            "hotkey_unregister_failed", kShortcutFailure);
        (void)context->Shutdown();
        return kShortcutFailure;
    }
    Log("shortcut_unregister", "passed", "replacement registration removed",
        environmentName, {}, kSuccess);
    if (!instance.Value().primary.has_value()) {
        Log("single_instance_release", "failed",
            "primary session token is missing", environmentName,
            "primary_registration_missing", kInstanceFailure);
        (void)context->Shutdown();
        return kInstanceFailure;
    }
    if (const auto released = instance.Value().primary->Release(); !released) {
        Log("single_instance_release", "failed", ErrorText(released.Error()),
            environmentName, "single_instance_release_failed",
            kInstanceFailure);
        (void)context->Shutdown();
        return kInstanceFailure;
    }
    Log("single_instance_release", "passed", "primary endpoint released",
        environmentName, {}, kSuccess);
    if (const auto shutdown = context->Shutdown(); !shutdown) {
        Log("shutdown", "failed", ErrorText(shutdown.Error()),
            environmentName, "desktop_shutdown_failed", kShutdownFailure);
        return kShutdownFailure;
    }
    Log("shutdown", "passed", "tray, shortcut, instance and backend stopped",
        environmentName, {}, kSuccess);
    Log("summary", "passed",
        std::string("tray_commands=") + std::to_string(trayCommandCount) +
            ",tray_double_clicks=" + std::to_string(trayDoubleClickCount) +
            ",shortcut_presses=" + std::to_string(shortcutPressedCount) +
            ",primary_activations=" +
                std::to_string(primaryActivationCount),
        environmentName, {}, kSuccess);
    return kSuccess;
}

#else

int main() { return 0; }

#endif
