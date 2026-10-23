#include "Application/FrameScheduler.h"
#include "Platform/WindowPlacementUtils.h"
#include "Platform/Win32/Win32ApplicationHost.h"
#include "Platform/Win32/Win32WindowSystem.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int g_Failures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        ++g_Failures;
        std::cerr << "[FAIL] " << message << '\n';
    } else {
        std::cout << "[PASS] " << message << '\n';
    }
}

bool Near(double lhs, double rhs) {
    return std::fabs(lhs - rhs) < 0.0001;
}

} // namespace

int main() {
    using namespace Engine;

    {
        WindowHandle handle{1, 1};
        Expect(handle.IsValid(), "WindowHandle generation is valid when index and generation are non-zero");
        WindowHandle stale{1, 2};
        Expect(handle != stale, "WindowHandle generation differentiates stale handles");
    }

    {
        WindowContext hidden{};
        hidden.window = {1, 1};
        hidden.visible = false;
        hidden.framePolicy = FramePolicy::Hidden;
        FrameScheduler scheduler;
        const FrameDecision decision = scheduler.Evaluate(hidden, false, false, 1.0);
        Expect(!decision.update && !decision.render, "Hidden window does not update or render");
    }

    {
        WindowContext context{};
        context.window = {1, 1};
        context.visible = true;
        context.framePolicy = FramePolicy::Static;
        context.dirty = false;
        FrameScheduler scheduler;
        FrameDecision decision = scheduler.Evaluate(context, false, false, 1.0);
        Expect(!decision.update && !decision.render, "Static clean window waits");
        context.dirty = true;
        decision = scheduler.Evaluate(context, false, false, 1.1);
        Expect(decision.update && decision.render, "Static dirty window renders once");
        context.dirty = false;
        decision = scheduler.Evaluate(context, false, true, 1.2);
        Expect(decision.update && decision.render, "Static clean window wakes for pending runtime activity");
    }

    {
        WindowContext context{};
        context.window = {2, 1};
        context.visible = true;
        context.framePolicy = FramePolicy::Active;
        FrameScheduler scheduler;
        FrameDecision first = scheduler.Evaluate(context, false, false, 1.0);
        context.dirty = false;
        FrameDecision second = scheduler.Evaluate(context, false, false, 1.001);
        FrameDecision third = scheduler.Evaluate(context, false, false, 1.020);
        Expect(first.update && first.render, "Active window renders first frame");
        Expect(!second.update && !second.render && second.waitSeconds > 0.0, "Active window waits before 60 FPS interval");
        Expect(third.update && third.render, "Active window renders after 60 FPS interval");
    }

    {
        WindowContext context{};
        context.window = {3, 1};
        context.visible = true;
        context.framePolicy = FramePolicy::Static;
        context.dirty = false;
        FrameScheduler scheduler;
        scheduler.RequestFrame(context.window, FrameReason::Realtime);
        const FrameDecision requested = scheduler.Evaluate(context, false, false, 2.0);
        scheduler.AcknowledgeFrame(context.window, 2.0);
        const FrameDecision idle = scheduler.Evaluate(context, false, false, 2.01);
        Expect(requested.render && requested.reason == FrameReason::Realtime,
            "RequestFrame wakes an on-demand window with its frame reason");
        Expect(!idle.render, "Acknowledged on-demand frame returns to idle");
    }

    {
        WindowContext context{};
        context.window = {4, 1};
        context.visible = true;
        context.minimized = true;
        context.framePolicy = FramePolicy::Static;
        context.dirty = true;
        FrameScheduler scheduler;
        const FrameDecision decision = scheduler.Evaluate(context, false, false, 3.0);
        Expect(!decision.update && !decision.render,
            "Minimized window never renders a pending frame");
        context.minimized = false;
        const FrameDecision restored = scheduler.Evaluate(context, false, false, 3.1);
        Expect(restored.render, "Restored window can render its pending frame");
    }

    {
        WindowContext context{};
        context.window = {5, 1};
        context.visible = true;
        context.framePolicy = FramePolicy::Static;
        context.dirty = false;
        FrameScheduler scheduler;
        const FrameDecision first = scheduler.Evaluate(context, true, false, 4.0);
        const FrameDecision paced = scheduler.Evaluate(context, true, false, 4.001);
        const FrameDecision next = scheduler.Evaluate(context, true, false, 4.02);
        Expect(first.render && first.reason == FrameReason::Animation,
            "Active animation starts a frame stream");
        Expect(!paced.render && paced.paced && paced.waitSeconds > 0.0,
            "Active animation is paced instead of spinning");
        Expect(next.render, "Active animation continues at the target cadence");
    }

    {
        PointI scaled = ScalePointForDpi({100, 80}, 1.25f);
        PointI unscaled = UnscalePointForDpi(scaled, 1.25f);
        Expect(scaled.x == 125 && scaled.y == 100, "DPI point scaling supports 125 percent");
        Expect(unscaled.x == 100 && unscaled.y == 80, "DPI point unscaling restores logical coordinates");
    }

    {
        PointI clamped = ClampWindowPositionToWorkArea({900, 700}, 250, 200, {0, 0, 1024, 768});
        Expect(clamped.x == 774 && clamped.y == 568, "Window position clamps to work area");
    }

#if defined(_WIN32)
    {
        Win32ApplicationHost host;
        Win32WindowSystem windows;
        WindowDesc desc{};
        desc.title = "XiE host test";
        desc.width = 120;
        desc.height = 80;
        desc.visible = false;
        WindowHandle first = windows.CreateWindow(desc);
        WindowHandle second = windows.CreateWindow(desc);
        Expect(first.IsValid() && second.IsValid() && first != second, "Win32WindowSystem creates multiple windows");
        windows.DestroyWindow(first);
        Expect(windows.GetNativeHandle(first) == 0, "Destroyed Win32 window invalidates stale generation");
        windows.DestroyWindow(second);
        host.RequestExit();
        host.PollEvents();
        Expect(host.ShouldExit(), "Win32ApplicationHost exits after request");
    }
#else
    std::cout << "[SKIP] Win32 multiple-window and host-close tests\n";
#endif

    if (g_Failures != 0) {
        std::cerr << "Runtime host tests failed: " << g_Failures << '\n';
        return EXIT_FAILURE;
    }

    std::cout << "Runtime host tests passed\n";
    return EXIT_SUCCESS;
}
