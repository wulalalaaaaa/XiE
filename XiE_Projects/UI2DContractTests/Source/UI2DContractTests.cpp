#include "Application/FrameScheduler.h"
#include "Application/WindowRuntimeRegistry.h"
#include "Input/InputEventQueue.h"
#include "UI2D/Animation/IUIAnimator.h"
#include "UI2D/Core/UISceneMutationQueue.h"
#include "UI2D/Input/IUIFocusManager.h"
#include "UI2D/Input/IUIHitTester.h"
#include "UI2D/Input/IUIInputRouter.h"
#include "UI2D/Layout/IUILayoutEngine.h"
#include "UI2D/Rendering/IUIRenderBuilder.h"
#include "UI2D/Runtime/UIWindowRuntime.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;
void Expect(bool value, const char* message) {
    std::cout << (value ? "[PASS] " : "[FAIL] ") << message << '\n';
    if (!value) ++failures;
}

struct CallLog { std::vector<std::string> values; };

class FakeLayout final : public Engine::UI2D::IUILayoutEngine {
public:
    explicit FakeLayout(CallLog* log = nullptr) : log(log) {}
    Engine::UI2D::UILayoutResult UpdateLayout(
        Engine::UI2D::UIScene&, const Engine::UI2D::UILayoutContext&) override {
        if (log) log->values.push_back("layout");
        ++calls;
        Engine::UI2D::UILayoutResult result;
        result.layoutChanged = true;
        result.visualInvalidated = true;
        result.hitTestInvalidated = true;
        result.measuredNodeCount = 1;
        result.arrangedNodeCount = 1;
        return result;
    }
    CallLog* log{}; int calls = 0;
};

class FakeHitTester final : public Engine::UI2D::IUIHitTester {
public:
    Engine::UI2D::UIHitTestResult HitTest(
        const Engine::UI2D::UIScene&,
        const Engine::UI2D::UIHitTestContext&,
        Engine::Vec2F) const override { return {}; }
};

class FakeRouter final : public Engine::UI2D::IUIInputRouter {
public:
    explicit FakeRouter(CallLog* log = nullptr) : log(log) {}
    Engine::UI2D::UIInputDispatchResult Dispatch(
        Engine::UI2D::UIScene&, std::span<const Engine::InputEvent> events,
        Engine::UI2D::UIInputDispatchContext&) override {
        if (log) log->values.push_back("input");
        ++calls;
        Engine::UI2D::UIInputDispatchResult result;
        result.consumedInputCount = static_cast<std::uint32_t>(events.size());
        return result;
    }
    void OnNodeInvalidated(Engine::UI2D::UIScene&, Engine::UI2D::UINodeHandle node) override {
        invalidated.push_back(node);
    }
    CallLog* log{}; int calls = 0; std::vector<Engine::UI2D::UINodeHandle> invalidated;
};

class FakeAnimator final : public Engine::UI2D::IUIAnimator {
public:
    explicit FakeAnimator(CallLog* log = nullptr) : log(log) {}
    Engine::UI2D::UIAnimationStartResult Start(
        Engine::UI2D::UIScene&, const Engine::UI2D::UIAnimationDesc&) override { return {}; }
    bool Pause(Engine::UI2D::UIAnimationHandle) override { return false; }
    bool Resume(Engine::UI2D::UIAnimationHandle) override { return false; }
    bool Cancel(Engine::UI2D::UIScene&, Engine::UI2D::UIAnimationHandle,
        Engine::UI2D::UIAnimationCancelMode) override { return false; }
    std::uint32_t CancelAllForNode(Engine::UI2D::UIScene&, Engine::UI2D::UINodeHandle,
        Engine::UI2D::UIAnimationCancelMode) override { return 0; }
    std::uint32_t CancelAll(Engine::UI2D::UIScene&, Engine::UI2D::UIAnimationCancelMode) override { return 0; }
    Engine::UI2D::UIAnimationUpdateResult Update(Engine::UI2D::UIScene&, double) override {
        if (log) log->values.push_back("animator");
        ++calls; return next;
    }
    bool HasActiveAnimations() const override { return active; }
    bool IsRunning(Engine::UI2D::UIAnimationHandle) const override { return false; }
    void OnNodeInvalidated(Engine::UI2D::UIScene&, Engine::UI2D::UINodeHandle node) override { invalidated.push_back(node); }
    CallLog* log{}; int calls = 0; bool active = false;
    Engine::UI2D::UIAnimationUpdateResult next{};
    std::vector<Engine::UI2D::UINodeHandle> invalidated;
};

class FakeRenderBuilder final : public Engine::UI2D::IUIRenderBuilder {
public:
    explicit FakeRenderBuilder(CallLog* log = nullptr) : log(log) {}
    Engine::UI2D::UIRenderResult Build(
        const Engine::UI2D::UIScene& scene,
        const Engine::UI2D::ResolvedUITheme&,
        Engine::DrawList2D& output,
        const Engine::UI2D::UIRenderContext&) override {
        if (log) log->values.push_back("render");
        output.Clear(); ++calls;
        Engine::UI2D::UIRenderResult result;
        result.success = true; result.drawListChanged = true;
        result.visitedNodeCount = static_cast<std::uint32_t>(scene.NodeCount());
        return result;
    }
    CallLog* log{}; int calls = 0;
};

struct FakeSet {
    explicit FakeSet(CallLog* log = nullptr)
        : layout(log), router(log), animator(log), render(log) {}
    FakeLayout layout;
    FakeHitTester hit;
    FakeRouter router;
    FakeAnimator animator;
    FakeRenderBuilder render;
    Engine::UI2D::UI2DServices Services() { return {layout, hit, router, render}; }
};

bool PublicHeadersAreBackendFree() {
    const std::filesystem::path root = std::filesystem::path(XIE_SOURCE_DIR) / "Engine" / "UI2D";
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (entry.path().extension() != ".h") continue;
        std::ifstream input(entry.path());
        const std::string text((std::istreambuf_iterator<char>(input)), {});
        if (text.find("windows.h") != std::string::npos || text.find("Windows.h") != std::string::npos ||
            text.find("GLFW/") != std::string::npos || text.find("glad/") != std::string::npos ||
            text.find("Backends/OpenGL") != std::string::npos || text.find("Backends/Software") != std::string::npos) return false;
    }
    return true;
}

bool PlatformContractsAreNativeHeaderFree() {
    const std::filesystem::path root = std::filesystem::path(XIE_SOURCE_DIR) / "Engine" / "Platform";
    const std::vector<std::filesystem::path> contracts = {
        "IApplicationHost.h", "IWindowSystem.h", "IRenderSurface.h", "NativeWindowHandle.h",
        "WindowDesc.h", "WindowHandle.h"};
    for (const auto& relative : contracts) {
        std::ifstream input(root / relative);
        const std::string text((std::istreambuf_iterator<char>(input)), {});
        if (text.find("windows.h") != std::string::npos || text.find("Windows.h") != std::string::npos ||
            text.find("GLFW/") != std::string::npos || text.find("GLFWwindow") != std::string::npos) return false;
    }
    return true;
}

} // namespace

int main() {
    using namespace Engine;
    using namespace Engine::UI2D;

    UIScene scene;
    const UINodeHandle stale = scene.CreateNode("stale");
    Expect(stale.IsValid() && scene.DestroyNode(stale) && scene.TryGet(stale) == nullptr,
        "UINodeHandle generation invalidates destroyed handles");
    const UINodeHandle replacement = scene.CreateNode("replacement");
    Expect(replacement.index == stale.index && replacement.generation != stale.generation,
        "reused node slots receive a new generation");

    const UINodeHandle parent = scene.CreateNode("parent");
    const UINodeHandle child = scene.CreateNode("child", parent);
    const UINodeHandle grandchild = scene.CreateNode("grandchild", child);
    Expect(scene.NodeCount() >= 4, "nodes are created and attached");
    Expect(!scene.ReparentNode(parent, grandchild), "cyclic parent relationships are rejected");
    Expect(scene.DestroyNode(parent) && !scene.TryGet(child) && !scene.TryGet(grandchild),
        "destroying a parent invalidates its full subtree");

    UISceneMutationQueue mutations;
    mutations.Enqueue(SetVisibilityMutation{replacement, false});
    mutations.Enqueue(SetVisibilityMutation{replacement, true});
    const auto snapshot = scene.LiveNodes();
    Expect(scene.TryGet(replacement)->visible && snapshot.size() == scene.NodeCount(),
        "queued mutations do not modify the container during traversal");
    const UISceneMutationResult ordered = mutations.Flush(scene);
    Expect(ordered.appliedCount == 2 && scene.TryGet(replacement)->visible,
        "mutation queue executes changes in enqueue order");
    mutations.Enqueue(DestroyNodeMutation{{9999, 1}});
    Expect(!mutations.Flush(scene).Succeeded(), "destroying a missing node reports an error");
    mutations.Enqueue(ReparentNodeMutation{replacement, replacement});
    Expect(!mutations.Flush(scene).Succeeded(), "mutation queue reports rejected cycles");

    InputEventQueue firstQueue({1, 1});
    InputEventQueue secondQueue({2, 1});
    Expect(firstQueue.Push(PointerMoveEvent{{1, 1}, {10, 10}}) &&
           !secondQueue.Push(PointerMoveEvent{{1, 1}, {10, 10}}) && secondQueue.Empty(),
        "input queues enforce per-window isolation");

    WindowRuntimeRegistry registry;
    WindowContext registered{}; registered.window = {20, 1};
    Expect(registry.Register(std::move(registered)).IsValid() && registry.Unregister({20, 1}),
        "window runtime registry registers and releases window state");
    WindowContext staleWindow{}; staleWindow.window = {20, 1};
    WindowContext nextWindow{}; nextWindow.window = {20, 2};
    Expect(!registry.Register(std::move(staleWindow)).IsValid() &&
           registry.Register(std::move(nextWindow)).IsValid() && registry.TryGet({20, 1}) == nullptr,
        "window runtime registry rejects stale generations");

    CallLog log;
    FakeSet services(&log);
    UIWindowRuntime runtime({1, 1}, services.Services(), MakeDefaultResolvedUITheme());
    Expect(runtime.EnqueueInput(WindowFocusEvent{{1, 1}, true}), "UIWindowRuntime accepts matching window input");
    UI2DUpdateResult first = runtime.Update(0.016, {640, 480}, 1.0f);
    Expect(first.drawListRebuilt && first.layoutUpdated && first.consumedInputCount == 1,
        "UIWindowRuntime constructs and completes an empty-scene update");
    Expect(log.values == std::vector<std::string>({"layout", "input", "render"}),
        "UIWindowRuntime validates layout before input, animation and render");

    log.values.clear();
    const int renderCalls = services.render.calls;
    UI2DUpdateResult staticUpdate = runtime.Update(0.016, {640, 480}, 1.0f);
    Expect(!staticUpdate.drawListRebuilt && services.render.calls == renderCalls,
        "a static clean scene does not rebuild its DrawList");

    runtime.SetVisible(false);
    log.values.clear();
    const int layoutCalls = services.layout.calls;
    runtime.Update(1.0, {800, 600}, 2.0f);
    Expect(log.values.empty() && services.layout.calls == layoutCalls,
        "hidden windows skip input dispatch, layout, animation and render building");
    Expect(!runtime.EnqueueInput(PointerMoveEvent{{1, 1}, {2, 3}}) &&
           runtime.EnqueueInput(WindowFocusEvent{{1, 1}, false}),
        "hidden windows discard pointer noise and retain bounded lifecycle input");

    runtime.SetVisible(true);
    runtime.Update(0.016, {800, 600}, 2.0f);
    const int afterResizeLayouts = services.layout.calls;
    runtime.SetTheme(std::make_shared<const ResolvedUITheme>());
    Expect(runtime.QueryFrameActivity().needsLayout && runtime.QueryFrameActivity().needsRender,
        "changing theme marks layout and visual state dirty");
    runtime.Update(0.016, {800, 600}, 2.0f);
    runtime.Update(0.016, {801, 600}, 2.0f);
    const int afterSizeChangeLayouts = services.layout.calls;
    runtime.Update(0.016, {801, 600}, 1.5f);
    Expect(services.layout.calls >= afterResizeLayouts + 2 && services.layout.calls == afterSizeChangeLayouts + 1,
        "theme, window size and DPI changes each trigger layout");

    const UINodeHandle notifiedParent = runtime.Scene().CreateNode("notify-parent");
    const UINodeHandle notifiedChild = runtime.Scene().CreateNode("notify-child", notifiedParent);
    runtime.Scene().SetFocusable(notifiedChild, true);
    runtime.RequestFocus(notifiedChild);
    runtime.Update(0.016, {801, 600}, 2.0f);
    runtime.Mutations().Enqueue(DestroyNodeMutation{notifiedParent});
    runtime.Update(0.016, {801, 600}, 2.0f);
    Expect(!runtime.FocusedNode().IsValid() && services.router.invalidated.size() >= 2,
        "node destruction notifies focus, animator and input router for every invalidated node");

    WindowContext window{};
    window.window = {7, 1}; window.visible = true; window.framePolicy = FramePolicy::Static;
    const UIFrameActivity initialActivity = UIWindowRuntime({7, 1}, services.Services(), nullptr).QueryFrameActivity();
    window.dirty = initialActivity.needsLayout || initialActivity.needsRender;
    FrameScheduler scheduler;
    const FrameDecision decision = scheduler.Evaluate(window, initialActivity.hasActiveAnimation,
        initialActivity.hasPendingInput || initialActivity.hasPendingMutation, 1.0);
    Expect(decision.update && decision.render, "UIFrameActivity maps cleanly to FrameScheduler inputs");

    FakeSet otherServices;
    UIWindowRuntime other({2, 1}, otherServices.Services(), nullptr);
    const UINodeHandle onlyFirst = runtime.Scene().CreateNode("first-only");
    runtime.Scene().SetFocusable(onlyFirst, true);
    runtime.RequestFocus(onlyFirst);
    runtime.Update(0.016, {801, 600}, 2.0f);
    Expect(other.Scene().TryGet(onlyFirst) == nullptr && !other.FocusedNode().IsValid(),
        "two UI windows keep scene and focus state independent");

    Expect(PublicHeadersAreBackendFree(), "UI2D public headers do not expose Win32, GLFW or renderer backends");
    Expect(PlatformContractsAreNativeHeaderFree(), "platform contract headers do not expose Win32 or GLFW types");

    std::cout << (failures == 0 ? "UI2D contract tests passed\n" : "UI2D contract tests failed\n");
    return failures == 0 ? 0 : 1;
}
