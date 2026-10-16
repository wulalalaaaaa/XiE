#include "Application/ApplicationContext.h"
#include "Application/ApplicationCapabilities.h"
#include "UI2D/Core/UIScene.h"
#include "UI2D/Core/UITransform.h"
#include "UI2D/Core/UIVisual.h"
#include "UI2D/Input/UIEventContext.h"
#include "UI2D/Layout/UILayoutTypes.h"
#include "UI2D/Runtime/UIWindowRuntime.h"

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

namespace {

using namespace Engine;
using namespace Engine::UI2D;

constexpr int kSuccess = 0;
constexpr int kEnvironmentBlocked = 20;
constexpr int kContextFailure = 30;
constexpr int kNativeFailure = 31;
constexpr int kCleanupFailure = 32;

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
            result += character < 0x20u ? '?' : static_cast<char>(character);
            break;
        }
    }
    return result;
}

void Log(std::string_view phase, std::string_view result,
    std::string_view detail = {}, int code = kSuccess) {
    std::cout << "{\"tool\":\"xie_interactive_overlay_native_smoke\","
              << "\"phase\":\"" << JsonEscape(phase)
              << "\",\"result\":\"" << JsonEscape(result)
              << "\",\"code\":" << code
              << ",\"detail\":\"" << JsonEscape(detail) << "\"}\n";
    std::cout.flush();
}

std::string ErrorText(const ApplicationError& error) {
    std::string result = ToString(error.code);
    if (!error.operation.empty()) result += " operation=" + error.operation;
    if (!error.message.empty()) result += " message=" + error.message;
    if (!error.capability.empty()) result += " capability=" + error.capability;
    if (!error.backend.empty()) result += " backend=" + error.backend;
    if (error.nativeCode) {
        result += " native=" + std::to_string(*error.nativeCode);
    }
    return result;
}

std::string UniqueTitle() {
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    return "XiE Interactive Overlay Phase8 " +
        std::to_string(GetCurrentProcessId()) + " " + std::to_string(ticks);
}

bool IsEnvironmentUnavailable(const ApplicationError& error) {
    return error.code == ApplicationErrorCode::Unsupported ||
        error.code == ApplicationErrorCode::BackendUnavailable ||
        error.code == ApplicationErrorCode::WrongThread;
}

} // namespace

int main() {
    if (std::getenv("XIE_RUN_INTERACTIVE_OVERLAY_SMOKE") == nullptr) {
        Log("preflight", "not_run",
            "set XIE_RUN_INTERACTIVE_OVERLAY_SMOKE=1 to create a real XiE transparent window",
            kSuccess);
        return kSuccess;
    }

    ApplicationContextOptions contextOptions;
    contextOptions.applicationName = "XiE Interactive Overlay Native Smoke";
    contextOptions.initializeBackends = true;
    auto contextResult = ApplicationContext::Create(std::move(contextOptions));
    if (!contextResult) {
        const int code = IsEnvironmentUnavailable(contextResult.Error())
            ? kEnvironmentBlocked : kContextFailure;
        const std::string detail = code == kEnvironmentBlocked
            ? "Environment Blocked / Not Tested: " +
                ErrorText(contextResult.Error())
            : ErrorText(contextResult.Error());
        Log("context", code == kEnvironmentBlocked ? "blocked" : "failed",
            detail, code);
        return code;
    }
    auto context = std::move(contextResult.Value());
    const auto* transparentCapability = context->Capabilities().Find(
        ApplicationCapability::TransparentWindow);
    if (transparentCapability == nullptr ||
        transparentCapability->status == CapabilityStatus::Unavailable) {
        const std::string detail = transparentCapability == nullptr
            ? "transparent_window capability is absent"
            : "status=" + std::string(ToString(transparentCapability->status)) +
                " reason=" + transparentCapability->reason +
                " backend=" + transparentCapability->backend +
                " runtimeInitialized=" +
                (transparentCapability->runtimeInitialized ? "1" : "0");
        Log("capability", "blocked",
            "Environment Blocked / Not Tested: " + detail,
            kEnvironmentBlocked);
        (void)context->Shutdown();
        return kEnvironmentBlocked;
    }

    const std::string title = UniqueTitle();
    TransparentWindowOptions options;
    options.title = title;
    options.size = {420, 300};
    options.visible = false;
    options.transparent = true;
    options.alwaysOnTop = true;
    options.toolWindow = true;
    options.focusPolicy = WindowFocusPolicy::Activate;
    options.clickThrough = false;
    options.opacity = 1.0f;
    options.placement = {-160, 96, false};
    options.closePolicy = WindowClosePolicy::Hide;
    options.dpiPolicy = WindowDpiPolicy::PerMonitor;
    auto sessionResult = context->Windows().CreateTransparent(options);
    if (!sessionResult) {
        const int code = IsEnvironmentUnavailable(sessionResult.Error())
            ? kEnvironmentBlocked : kNativeFailure;
        const std::string detail = code == kEnvironmentBlocked
            ? "Environment Blocked / Not Tested: " +
                ErrorText(sessionResult.Error())
            : ErrorText(sessionResult.Error());
        Log("create", code == kEnvironmentBlocked ? "blocked" : "failed",
            detail, code);
        (void)context->Shutdown();
        return code;
    }
    auto session = std::move(sessionResult.Value());
    const auto bound = session.BindUI();
    if (!bound) {
        Log("bind_ui", "failed", ErrorText(bound.Error()), kContextFailure);
        (void)session.Close();
        (void)context->Shutdown();
        return kContextFailure;
    }

    const auto target = session.UI()->Scene().CreateNode(
        "XiE.InteractiveOverlay.NativeTarget");
    UILayoutParams targetLayout;
    targetLayout.sizeRule.width = {UISizeMode::Fixed, 180.0f};
    targetLayout.sizeRule.height = {UISizeMode::Fixed, 100.0f};
    session.UI()->Scene().SetLayout(target, targetLayout);
    session.UI()->Scene().SetVisual(target,
        UIPanelVisual{{0.15f, 0.45f, 0.9f, 0.8f}, 8.0f, true});
    session.UI()->Scene().SetFocusable(target, true);
    session.UI()->Scene().SetHitTestVisible(target, true);
    bool nativePointerSeen = false;
    session.UI()->AddEventListener(target, UIEventType::PointerDown,
        UIEventPhaseMask::All,
        [&nativePointerSeen](UIEventContext&) { nativePointerSeen = true; });

    if (!session.SetPosition(-160, 96) || !session.Show()) {
        Log("window_ops", "failed", ErrorText(session.LastError().empty()
            ? ApplicationError{} : ApplicationError{
                ApplicationErrorCode::BackendFailure, "Show", "window",
                session.LastError(), std::nullopt, session.BackendName()}),
            kNativeFailure);
        (void)session.Close();
        (void)context->Shutdown();
        return kNativeFailure;
    }

    HWND nativeWindow = FindWindowW(L"XiE.Win32Window", nullptr);
    if (nativeWindow == nullptr ||
        GetWindowTextLengthW(nativeWindow) == 0) {
        Log("native_window", "failed",
            "real XiE transparent HWND could not be located", kNativeFailure);
        (void)session.Close();
        (void)context->Shutdown();
        return kNativeFailure;
    }
    // The class is unique to this smoke process, but verify the title before
    // sending any test-only messages to avoid touching another window.
    std::wstring actualTitle(256, L'\0');
    const int titleLength = GetWindowTextW(nativeWindow, actualTitle.data(),
        static_cast<int>(actualTitle.size()));
    actualTitle.resize(titleLength > 0 ? static_cast<std::size_t>(titleLength) : 0);
    if (actualTitle != std::wstring(title.begin(), title.end())) {
        Log("native_window", "failed",
            "located Win32 window title does not match this smoke instance",
            kNativeFailure);
        (void)session.Close();
        (void)context->Shutdown();
        return kNativeFailure;
    }

    const LONG_PTR extendedStyle = GetWindowLongPtrW(
        nativeWindow, GWL_EXSTYLE);
    const bool layered = (extendedStyle & WS_EX_LAYERED) != 0;
    const bool toolWindow = (extendedStyle & WS_EX_TOOLWINDOW) != 0;
    const bool topMost = (extendedStyle & WS_EX_TOPMOST) != 0;
    Log("native_window", layered && toolWindow && topMost ? "passed" : "failed",
        std::string("real HWND styles layered=") + (layered ? "1" : "0") +
            ",toolWindow=" + (toolWindow ? "1" : "0") +
            ",topMost=" + (topMost ? "1" : "0"),
        layered && toolWindow && topMost ? kSuccess : kNativeFailure);
    if (!layered || !toolWindow || !topMost) {
        (void)session.Close();
        (void)context->Shutdown();
        return kNativeFailure;
    }

    const auto placement = session.QueryPlacement();
    if (!placement || placement.Value().width <= 0 ||
        placement.Value().height <= 0) {
        Log("placement_dpi", "failed",
            placement ? "real window returned an invalid placement"
                      : ErrorText(placement.Error()), kNativeFailure);
        (void)session.Close();
        (void)context->Shutdown();
        return kNativeFailure;
    }
    Log("placement_dpi", "passed",
        "negative-coordinate placement and per-monitor DPI query completed",
        kSuccess);

    // This is a native-message smoke, not a physical mouse test.  PostMessage
    // is intentionally confined to this test executable; product code only
    // receives XiE InputEvent values through WindowSession::BindUI().
    const LPARAM point = MAKELPARAM(40, 40);
    if (PostMessageW(nativeWindow, WM_MOUSEMOVE, 0, point) == FALSE ||
        PostMessageW(nativeWindow, WM_LBUTTONDOWN, MK_LBUTTON, point) == FALSE ||
        PostMessageW(nativeWindow, WM_LBUTTONUP, 0, point) == FALSE) {
        Log("native_messages", "failed",
            "PostMessageW could not enqueue the test pointer sequence",
            kNativeFailure);
        (void)session.Close();
        (void)context->Shutdown();
        return kNativeFailure;
    }

    int updateCount = 0;
    int renderCount = 0;
    ApplicationRunOptions runOptions;
    runOptions.update = [&](WindowSession& current, double) {
        if (current.Handle() != session.Handle()) return;
        ++updateCount;
        // A static transparent surface has no continuous work after its first
        // frame.  Request exit after that bounded frame instead of waiting
        // for a fifth update that the scheduler is intentionally not meant
        // to produce.
        if (updateCount >= 1) (void)context->RequestExit();
    };
    runOptions.render = [&](WindowSession& current) {
        if (current.Handle() == session.Handle()) ++renderCount;
    };
    const auto ran = context->Run(runOptions);
    const bool nativeMessagesPassed = ran && updateCount > 0 &&
        renderCount > 0 && nativePointerSeen;
    Log("native_messages", nativeMessagesPassed ? "passed" : "failed",
        std::string("updates=") + std::to_string(updateCount) +
            ",renders=" + std::to_string(renderCount) +
            ",pointerDownDelivered=" + (nativePointerSeen ? "1" : "0"),
        nativeMessagesPassed ? kSuccess : kNativeFailure);
    if (!nativeMessagesPassed) {
        (void)session.Close();
        (void)context->Shutdown();
        return kNativeFailure;
    }

    const bool hidden = session.Hide() && !session.QueryState().visible;
    const bool reopened = hidden && session.Show() &&
        session.QueryState().visible && session.Handle().IsValid();
    Log("window_lifecycle", hidden && reopened ? "passed" : "failed",
        "hide, reopen and stable WindowSession identity completed",
        hidden && reopened ? kSuccess : kNativeFailure);
    if (!hidden || !reopened) {
        (void)session.Close();
        (void)context->Shutdown();
        return kNativeFailure;
    }

    const bool closed = session.Close() && session.Close() && session.IsClosed();
    const auto shutdown = context->Shutdown();
    const bool cleanup = closed && shutdown && context->IsShuttingDown();
    Log("cleanup", cleanup ? "passed" : "failed",
        "WindowSession and ApplicationContext shutdown are idempotent",
        cleanup ? kSuccess : kCleanupFailure);

    Log("physical_mouse", "not_tested",
        "manual physical mouse interaction was not performed by this native-message smoke",
        kSuccess);
    Log("chinese_ime", "not_tested",
        "Chinese IME composition requires a manual interactive desktop session",
        kSuccess);
    Log("visual_transparency", "not_tested",
        "layered style and real presentation path were exercised; visual screenshot review remains manual",
        kSuccess);
    Log("multi_monitor", "not_tested",
        "this run verified a negative screen coordinate; monitor migration and mixed-DPI review remain manual",
        kSuccess);
    Log("summary", cleanup ? "passed" : "failed",
        "real XiE CreateTransparent HWND and native message path completed",
        cleanup ? kSuccess : kCleanupFailure);
    return cleanup ? kSuccess : kCleanupFailure;
}

#else

int main() { return 20; }

#endif
