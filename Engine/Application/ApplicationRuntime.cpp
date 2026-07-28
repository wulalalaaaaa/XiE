#include "Application/ApplicationRuntime.h"

#include "Platform/IApplicationHost.h"
#include "Platform/IWindowSystem.h"

#include <chrono>
#include <vector>

namespace Engine {
namespace {
double NowSeconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
}

ApplicationRuntime::ApplicationRuntime(ApplicationServices services) : m_Services(services) {}
ApplicationRuntime::~ApplicationRuntime() {
    m_Commands.Close();
    std::vector<WindowHandle> handles;
    m_Registry.ForEachActiveWindow([&](WindowContext& window) { handles.push_back(window.window); });
    for (WindowHandle handle : handles) {
        // Destroy window-owned runtime state and pipelines before the native window.
        (void)m_Registry.Unregister(handle);
        m_Services.windows.DestroyWindow(handle);
    }
}

void ApplicationRuntime::Run(const ApplicationWindowCallbacks& callbacks) {
    double previous = NowSeconds();
    while (!m_Services.host.ShouldExit()) {
        m_Services.host.PollEvents();
        const bool hadCommands = m_Commands.ExecutePending(*this) != 0;
        const double now = NowSeconds();
        const double delta = now - previous;
        previous = now;
        double shortestWait = 0.25;
        bool didWork = false;
        std::vector<WindowHandle> closeRequests;

        m_Registry.ForEachActiveWindow([&](WindowContext& window) {
            window.visible = m_Services.windows.IsVisible(window.window);
            window.closeRequested = m_Services.windows.IsCloseRequested(window.window);
            window.dpiScale = m_Services.windows.GetDpiScale(window.window);
            if (window.closeRequested) {
                closeRequests.push_back(window.window);
                return;
            }
            const ApplicationWindowCallbacks::Activity activity = callbacks.queryActivity
                ? callbacks.queryActivity(window)
                : ApplicationWindowCallbacks::Activity{};
            const FrameDecision decision = m_Scheduler.Evaluate(
                window, activity.hasActiveAnimation, hadCommands || activity.hasPendingWork, now);
            if (decision.update && callbacks.update) callbacks.update(window, delta);
            if (decision.render && callbacks.render) {
                callbacks.render(window);
                if (window.renderPipeline) window.renderPipeline->Present();
            }
            didWork |= decision.update || decision.render;
            if (decision.waitSeconds > 0.0 && decision.waitSeconds < shortestWait) shortestWait = decision.waitSeconds;
        });

        for (WindowHandle handle : closeRequests) {
            (void)m_Registry.Unregister(handle);
            m_Services.windows.DestroyWindow(handle);
        }
        if (m_Registry.Size() == 0) {
            RequestExit();
        } else if (!didWork) {
            m_Services.host.WaitForEventsTimeout(shortestWait);
        }
    }
}

void ApplicationRuntime::RequestExit() { m_Services.host.RequestExit(); }

} // namespace Engine
