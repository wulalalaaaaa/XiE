#pragma once

#include "Application/ApplicationCapabilities.h"
#include "Application/ApplicationResult.h"
#include "Application/DesktopServices.h"
#include "Application/ApplicationWindowProvider.h"
#include "Application/WindowComposition.h"
#include "Image/ImageServices.h"

#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace Engine {

struct ApplicationContextOptions {
    std::string applicationName = "XiE Application";
    bool initializeBackends = true;
    ApplicationRuntime* runtime = nullptr;

    // Optional desktop services are owned by the context after Create().  A
    // normal XiE game leaves this empty, so shell/IPC code is not linked.
    std::unique_ptr<IDesktopBackend> desktopBackend;

    // Optional image services are owned by the context after Create().
    // Windows WIC/clipboard implementations are injected only by consumers
    // that explicitly opt into those backend targets.
    std::shared_ptr<ImageServices> imageServices;
};

struct ApplicationRunOptions {
    std::function<void(WindowSession&, double)> update;
    std::function<void(WindowSession&)> render;
    std::function<WindowFrameActivity(const WindowSession&)> queryActivity;
};

namespace Detail {
struct ApplicationCompositionState;
struct WindowSessionState;
}

class ApplicationContext final {
public:
    static ApplicationResult<std::unique_ptr<ApplicationContext>> Create(
        ApplicationContextOptions options = {});
    ~ApplicationContext() noexcept;

    ApplicationContext(const ApplicationContext&) = delete;
    ApplicationContext& operator=(const ApplicationContext&) = delete;
    ApplicationContext(ApplicationContext&&) = delete;
    ApplicationContext& operator=(ApplicationContext&&) = delete;

    [[nodiscard]] const CapabilitySnapshot& Capabilities() const noexcept;
    [[nodiscard]] WindowComposition& Windows() noexcept;
    [[nodiscard]] const WindowComposition& Windows() const noexcept;
    [[nodiscard]] DesktopServices& Desktop() noexcept;
    [[nodiscard]] const DesktopServices& Desktop() const noexcept;
    [[nodiscard]] ImageServices* Images() noexcept;
    [[nodiscard]] const ImageServices* Images() const noexcept;

    ApplicationResult<ApplicationProviderRegistration>
        RegisterWindowProvider(IApplicationWindowProvider& provider);

    // Used when an application deliberately creates the context without a
    // window backend first (for example to reject a secondary instance before
    // constructing its full UI/database graph).
    ApplicationResult<void> InitializeBackends();
    ApplicationResult<void> PumpDesktopEvents();
    // Pump native messages for windows owned by this Context. An outer host
    // may call this before ProcessWindowEvents() while it still owns the
    // application loop; it never creates a second loop or exposes a handle.
    ApplicationResult<void> PumpWindowEvents();

    // Process close/focus/visibility state for sessions after an outer host
    // has already pumped its native window events.  This deliberately does
    // not poll GLFW or Win32 itself; it lets a migrating host keep one outer
    // loop while XiE remains the owner of session close policy.
    ApplicationResult<void> ProcessWindowEvents();

    ApplicationResult<void> Run(
        const ApplicationRunOptions& options = {});
    ApplicationResult<void> Wait(double timeoutSeconds = -1.0);
    ApplicationResult<void> Wake();
    // Connect an outer application host to the same wake signal used by
    // DesktopServices.  The callback is optional, portable, and borrowed
    // only as a callable value; it never exposes a native handle or backend
    // object.  It is useful while a product still owns a legacy outer loop.
    ApplicationResult<void> SetExternalWakeCallback(
        std::function<void()> callback);
    ApplicationResult<void> RequestExit();
    ApplicationResult<void> Shutdown();

    // A narrow bridge for a host that still owns the outer loop.  It keeps
    // UI update/render composition in XiE while the host migration proceeds.
    ApplicationResult<WindowFrameResult> ProcessWindowFrame(
        WindowSession& session, double deltaSeconds);
    ApplicationResult<WindowFrameActivity> QueryWindowActivity(
        const WindowSession& session) const;
    ApplicationResult<Render2DStats> RenderWindow(WindowSession& session);
    // The no-argument form preserves the legacy render-only bridge.  The
    // explicit form is for an outer host that is not the owner of this
    // session's render pipeline and therefore must request presentation here.
    ApplicationResult<Render2DStats> RenderWindow(
        WindowSession& session, bool present);

    [[nodiscard]] bool IsRunning() const noexcept;
    [[nodiscard]] bool IsShuttingDown() const noexcept;

private:
    ApplicationContext(
        std::shared_ptr<Detail::ApplicationCompositionState> state,
        std::unique_ptr<IDesktopBackend> desktopBackend,
        std::unique_ptr<WindowComposition> windows);
    static WindowSession MakeSessionView(
        const std::shared_ptr<Detail::WindowSessionState>& state);
    ApplicationResult<Render2DStats> RenderWindowInternal(
        WindowSession& session, bool present);

    std::shared_ptr<Detail::ApplicationCompositionState> m_State;
    DesktopServices m_Desktop;
    mutable CapabilitySnapshot m_Capabilities;
    std::unique_ptr<WindowComposition> m_Windows;
};

} // namespace Engine
