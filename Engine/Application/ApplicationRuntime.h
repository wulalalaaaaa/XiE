#pragma once

#include "Application/ApplicationCommandQueue.h"
#include "Application/ApplicationServices.h"
#include "Application/FrameScheduler.h"
#include "Application/WindowRuntimeRegistry.h"

#include <functional>

namespace Engine {

struct ApplicationWindowCallbacks {
    std::function<void(WindowContext&, double)> update;
    std::function<void(WindowContext&)> render;
    struct Activity {
        bool hasActiveAnimation = false;
        bool hasPendingWork = false;
    };
    std::function<Activity(const WindowContext&)> queryActivity;

    // Optional composition hooks.  They keep specialized window systems
    // behind the application runtime without changing the legacy standard
    // window callback shape.  The hooks are deliberately expressed only in
    // XiE types; platform handles remain in backend implementations.
    std::function<bool(WindowContext&)> close;
    std::function<void()> pollExternal;
    std::function<bool()> hasExternalWindows;
    std::function<Activity()> queryExternalActivity;
    std::function<void(double)> updateExternal;
    std::function<void()> renderExternal;
};

// Platform-neutral multi-window loop. Project composition supplies content callbacks.
class ApplicationRuntime {
public:
    explicit ApplicationRuntime(ApplicationServices services);
    ~ApplicationRuntime();

    WindowRuntimeRegistry& Windows() noexcept { return m_Registry; }
    ApplicationCommandQueue& Commands() noexcept { return m_Commands; }
    ApplicationServices Services() const noexcept { return m_Services; }

    void Run(const ApplicationWindowCallbacks& callbacks);
    void RequestExit();

private:
    ApplicationServices m_Services;
    WindowRuntimeRegistry m_Registry;
    ApplicationCommandQueue m_Commands;
    FrameScheduler m_Scheduler;
};

} // namespace Engine
