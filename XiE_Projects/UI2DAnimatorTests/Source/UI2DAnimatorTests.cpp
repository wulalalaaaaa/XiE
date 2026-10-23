#include "UI2D/Animation/BasicUIAnimator.h"
#include "UI2D/Animation/UIAnimatedProperties.h"
#include "UI2D/Core/UIScene.h"
#include "UI2D/HitTest/BasicUIHitTester.h"
#include "UI2D/Input/BasicUIInputRouter.h"
#include "UI2D/Layout/BasicLayoutEngine.h"
#include "UI2D/Rendering/BasicUIRenderBuilder.h"
#include "UI2D/Runtime/UIWindowRuntime.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace {
using namespace Engine;
using namespace Engine::UI2D;

int failures = 0;
void Expect(bool condition, const char* message) {
    std::cout << (condition ? "[PASS] " : "[FAIL] ") << message << '\n';
    if (!condition) ++failures;
}
bool Near(float a, float b, float epsilon = 0.001f) { return std::abs(a - b) <= epsilon; }
bool Near(Vec2F a, Vec2F b) { return Near(a.x, b.x) && Near(a.y, b.y); }

UILayoutParams Fixed(float width, float height) {
    UILayoutParams result;
    result.sizeRule.width = {UISizeMode::Fixed, width};
    result.sizeRule.height = {UISizeMode::Fixed, height};
    return result;
}

struct Fixture {
    UIScene scene;
    BasicUIAnimator animator;
    UINodeHandle node = scene.CreateNode("animated");
    Fixture() {
        scene.SetLayout(node, Fixed(100, 80));
        UITransform transform;
        transform.position = {10, 20};
        transform.size = {100, 80};
        scene.SetTransform(node, transform);
    }
};

struct RuntimeFixture {
    BasicLayoutEngine layout;
    BasicUIHitTester hit;
    BasicUIInputRouter router;
    BasicUIRenderBuilder render;
    UIWindowRuntime runtime;

    explicit RuntimeFixture(WindowHandle window = {92,1})
        : runtime(window, {layout, hit, router, render}, nullptr) {}

    UINodeHandle AddPanel(const char* name, UINodeHandle parent,
        float x, float y, float width, float height, Color4f color = {}) {
        UINodeHandle node = runtime.Scene().CreateNode(name, parent);
        runtime.Scene().SetLayout(node, Fixed(width, height));
        UITransform transform;
        transform.position = {x,y}; transform.size = {width,height};
        runtime.Scene().SetTransform(node, transform);
        runtime.Scene().SetVisual(node, UIPanelVisual{color});
        return node;
    }
};

UIAnimationDesc VecAnimation(UINodeHandle node, UIAnimatedProperty property, Vec2F from, Vec2F to) {
    UIAnimationDesc desc;
    desc.node = node; desc.property = property; desc.from = from; desc.to = to;
    desc.durationSeconds = 1.0; desc.fillMode = UIAnimationFillMode::Forwards;
    return desc;
}
UIAnimationDesc ScalarAnimation(UINodeHandle node, UIAnimatedProperty property, float from, float to) {
    UIAnimationDesc desc;
    desc.node = node; desc.property = property; desc.from = from; desc.to = to;
    desc.durationSeconds = 1.0; desc.fillMode = UIAnimationFillMode::Forwards;
    return desc;
}

void TestMathAndCreation() {
    Fixture f;
    for (UIEasing easing : {UIEasing::Linear, UIEasing::EaseInQuad, UIEasing::EaseOutQuad,
        UIEasing::EaseInOutQuad, UIEasing::EaseInCubic, UIEasing::EaseOutCubic,
        UIEasing::EaseInOutCubic, UIEasing::SmoothStep}) {
        Expect(Near(EvaluateEasing(easing, 0), 0) && Near(EvaluateEasing(easing, 1), 1) &&
            std::isfinite(EvaluateEasing(easing, 0.37f)), "easing endpoints and finite output");
    }
    const UIAnimationValue mid = InterpolateAnimationValue(Vec2F{0, 10}, Vec2F{10, 30}, 0.5f);
    Expect(Near(std::get<Vec2F>(mid), {5, 20}), "vector interpolation at 50 percent");
    const Color4f colorMid = std::get<Color4f>(InterpolateAnimationValue(
        Color4f{0,0.2f,0.4f,0.6f}, Color4f{1,0.8f,0.6f,0.2f}, 0.5f));
    Expect(Near(colorMid.r, 0.5f) && Near(colorMid.g, 0.5f) &&
        Near(colorMid.b, 0.5f) && Near(colorMid.a, 0.4f), "color interpolation at 50 percent");

    UIAnimationStartResult started = f.animator.Start(
        f.scene, VecAnimation(f.node, UIAnimatedProperty::Position, {10,20}, {110,220}));
    Expect(started.success && started.handle.IsValid(), "valid animation returns generation handle");
    UIAnimationDesc bad = ScalarAnimation(f.node, UIAnimatedProperty::Position, 0, 1);
    Expect(!f.animator.Start(f.scene, bad).success, "property value mismatch is rejected");
    bad = ScalarAnimation({}, UIAnimatedProperty::Opacity, 0, 1);
    Expect(!f.animator.Start(f.scene, bad).success, "invalid node is rejected");
    bad = ScalarAnimation(f.node, UIAnimatedProperty::Opacity, 0, 1); bad.durationSeconds = -1;
    Expect(!f.animator.Start(f.scene, bad).success, "negative duration is rejected");
    UIAnimationDesc nonFinite;
    nonFinite.node = f.node; nonFinite.property = UIAnimatedProperty::ColorMultiplier;
    nonFinite.from = Color4f{};
    nonFinite.to = Color4f{std::numeric_limits<float>::quiet_NaN(),1,1,1};
    Expect(!f.animator.Start(f.scene, nonFinite).success, "non-finite animation values are rejected");
    f.animator.Update(f.scene, 0.5);
    Expect(Near(ResolveAnimatedProperties(*f.scene.TryGet(f.node)).position, {60,120}),
        "position advances deterministically to 50 percent");
    f.animator.Update(f.scene, 1.0);
    Expect(!f.animator.IsRunning(started.handle) &&
        Near(ResolveAnimatedProperties(*f.scene.TryGet(f.node)).position, {110,220}),
        "large delta completes and forwards fill persists");
    UIAnimationDesc sizeClamp = VecAnimation(
        f.node, UIAnimatedProperty::VisualSize, {100,80}, {-100,-40});
    f.animator.Start(f.scene, sizeClamp); f.animator.Update(f.scene, 1.0);
    Expect(Near(ResolveAnimatedProperties(*f.scene.TryGet(f.node)).visualSize, {0,0}),
        "VisualSize interpolation is clamped non-negative");
    ClearAnimatedOverride(f.scene, f.node, UIAnimatedProperty::VisualSize);
    UIAnimationDesc opacityClamp = ScalarAnimation(
        f.node, UIAnimatedProperty::Opacity, 1, -1);
    f.animator.Start(f.scene, opacityClamp); f.animator.Update(f.scene, 1.0);
    Expect(Near(ResolveAnimatedProperties(*f.scene.TryGet(f.node)).opacity, 0),
        "Opacity animation is clamped to the valid range");
    ClearAnimatedOverride(f.scene, f.node, UIAnimatedProperty::Opacity);
    UIAnimationStartResult reused = f.animator.Start(
        f.scene, VecAnimation(f.node, UIAnimatedProperty::Position, {0,0}, {1,1}));
    Expect(reused.handle.index == started.handle.index && reused.handle.generation != started.handle.generation,
        "released slot reuse increments generation");
    BasicUIAnimator other;
    Expect(!other.Pause(reused.handle), "animation handles are animator-local");
}

void TestCurrentReplaceQueue() {
    Fixture f;
    bool canceled = false, oldCompleted = false, completed = false;
    UIAnimationDesc first = VecAnimation(f.node, UIAnimatedProperty::Scale, {1,1}, {2,2});
    first.onCanceled = [&](UIAnimationHandle, UINodeHandle) { canceled = true; };
    first.onCompleted = [&](UIAnimationHandle, UINodeHandle) { oldCompleted = true; };
    const auto firstHandle = f.animator.Start(f.scene, first).handle;
    f.animator.Update(f.scene, 0.4);
    const Vec2F before = ResolveAnimatedProperties(*f.scene.TryGet(f.node)).scale;
    UIAnimationDesc replacement = VecAnimation(f.node, UIAnimatedProperty::Scale, {}, {1,1});
    replacement.fromMode = UIAnimationFromMode::Current;
    const auto replacementResult = f.animator.Start(f.scene, replacement);
    Expect(replacementResult.success && Near(ResolveAnimatedProperties(*f.scene.TryGet(f.node)).scale, before),
        "Current replacement preserves the current value without a jump");
    Expect(!f.animator.IsRunning(firstHandle) && canceled && !oldCompleted,
        "Replace invalidates the prior handle, skips Completion and invokes Cancel callback");

    UIAnimationDesc reject = replacement; reject.replaceMode = UIAnimationReplaceMode::Reject;
    Expect(!f.animator.Start(f.scene, reject).success, "Reject refuses an occupied property channel");
    UIAnimationDesc queued = VecAnimation(f.node, UIAnimatedProperty::Scale, {3,3}, {4,4});
    queued.replaceMode = UIAnimationReplaceMode::Queue;
    queued.onCompleted = [&](UIAnimationHandle, UINodeHandle) { completed = true; };
    const auto queuedHandle = f.animator.Start(f.scene, queued).handle;
    Expect(!f.animator.IsRunning(queuedHandle) && Near(ResolveAnimatedProperties(*f.scene.TryGet(f.node)).scale, before),
        "queued animation does not write before activation");
    f.animator.Update(f.scene, 1.0);
    Expect(f.animator.IsRunning(queuedHandle) && Near(ResolveAnimatedProperties(*f.scene.TryGet(f.node)).scale, {3,3}),
        "queue activates in creation order after prior completion");
    f.animator.Update(f.scene, 1.0);
    Expect(completed && Near(ResolveAnimatedProperties(*f.scene.TryGet(f.node)).scale, {4,4}),
        "queued animation completes normally");
}

void TestDelayFillLoopDirection() {
    Fixture f;
    UIAnimationDesc delayed = ScalarAnimation(f.node, UIAnimatedProperty::Opacity, 0.2f, 1.0f);
    delayed.delaySeconds = 1.0; delayed.fillMode = UIAnimationFillMode::None;
    const auto delayedHandle = f.animator.Start(f.scene, delayed).handle;
    f.animator.Update(f.scene, 0.5);
    Expect(Near(ResolveAnimatedProperties(*f.scene.TryGet(f.node)).opacity, 1.0f) &&
        f.animator.HasActiveAnimations() && f.animator.Pause(delayedHandle) &&
        !f.animator.HasActiveAnimations() && f.animator.Resume(delayedHandle),
        "None fill leaves base during delay, while Delay remains pausable Active work");
    f.animator.CancelAll(f.scene, UIAnimationCancelMode::RestoreBase);
    delayed.fillMode = UIAnimationFillMode::Backwards;
    f.animator.Start(f.scene, delayed); f.animator.Update(f.scene, 0.5);
    Expect(Near(ResolveAnimatedProperties(*f.scene.TryGet(f.node)).opacity, 0.2f),
        "Backwards fill applies starting value during delay");
    f.animator.CancelAll(f.scene, UIAnimationCancelMode::RestoreBase);

    UIAnimationDesc alternate = ScalarAnimation(f.node, UIAnimatedProperty::Rotation, 0, 10);
    alternate.direction = UIAnimationDirection::Alternate;
    alternate.iterationCount = 2;
    f.animator.Start(f.scene, alternate); f.animator.Update(f.scene, 2.0);
    Expect(Near(ResolveAnimatedProperties(*f.scene.TryGet(f.node)).rotationRadians, 0),
        "Alternate final direction ends at from on an even iteration count");
    UIAnimationDesc reverse = ScalarAnimation(f.node, UIAnimatedProperty::Rotation, 0, 10);
    reverse.direction = UIAnimationDirection::Reverse; reverse.replaceMode = UIAnimationReplaceMode::Replace;
    f.animator.Start(f.scene, reverse); f.animator.Update(f.scene, 0.25);
    Expect(Near(ResolveAnimatedProperties(*f.scene.TryGet(f.node)).rotationRadians, 7.5f),
        "Reverse plays from to toward from");
    f.animator.CancelAll(f.scene, UIAnimationCancelMode::RestoreBase);
    UIAnimationDesc alternateReverse = ScalarAnimation(f.node, UIAnimatedProperty::Rotation, 0, 10);
    alternateReverse.direction = UIAnimationDirection::AlternateReverse;
    alternateReverse.iterationCount = 2;
    f.animator.Start(f.scene, alternateReverse); f.animator.Update(f.scene, 2.0);
    Expect(Near(ResolveAnimatedProperties(*f.scene.TryGet(f.node)).rotationRadians, 10),
        "AlternateReverse ends at to after an even iteration count");
    f.animator.CancelAll(f.scene, UIAnimationCancelMode::RestoreBase);
    UIAnimationDesc multi = ScalarAnimation(f.node, UIAnimatedProperty::Rotation, 0, 10);
    multi.iterationCount = 3;
    f.animator.Start(f.scene, multi); f.animator.Update(f.scene, 2.5);
    Expect(Near(ResolveAnimatedProperties(*f.scene.TryGet(f.node)).rotationRadians, 5),
        "Normal direction resolves the correct progress after a multi-loop delta");
    f.animator.CancelAll(f.scene, UIAnimationCancelMode::RestoreBase);
    UIAnimationDesc infinite = ScalarAnimation(f.node, UIAnimatedProperty::Rotation, 0, 10);
    infinite.infinite = true; infinite.direction = UIAnimationDirection::Alternate;
    const auto infiniteHandle = f.animator.Start(f.scene, infinite).handle;
    f.animator.Update(f.scene, 7.25);
    Expect(f.animator.IsRunning(infiniteHandle) && Near(ResolveAnimatedProperties(*f.scene.TryGet(f.node)).rotationRadians, 7.5f),
        "infinite alternate survives a multi-iteration delta");
}

void TestPauseCancelCallbacks() {
    Fixture f;
    auto desc = ScalarAnimation(f.node, UIAnimatedProperty::Opacity, 1, 0);
    const auto handle = f.animator.Start(f.scene, desc).handle;
    f.animator.Update(f.scene, 0.25);
    const float paused = ResolveAnimatedProperties(*f.scene.TryGet(f.node)).opacity;
    Expect(f.animator.Pause(handle) && !f.animator.HasActiveAnimations(),
        "only paused animations allow Static scheduling");
    f.animator.Update(f.scene, 0.5);
    Expect(Near(ResolveAnimatedProperties(*f.scene.TryGet(f.node)).opacity, paused),
        "paused animation does not advance and keeps override");
    Expect(f.animator.Resume(handle) && f.animator.HasActiveAnimations(), "resume reactivates animation");
    f.animator.Update(f.scene, 0.25);
    Expect(Near(ResolveAnimatedProperties(*f.scene.TryGet(f.node)).opacity, 0.5f), "resume continues elapsed time");
    Expect(f.animator.Cancel(f.scene, handle, UIAnimationCancelMode::KeepCurrent) &&
        Near(ResolveAnimatedProperties(*f.scene.TryGet(f.node)).opacity, 0.5f),
        "KeepCurrent cancel retains evaluated override");
    Expect(!f.animator.Cancel(f.scene, handle, UIAnimationCancelMode::RestoreBase),
        "stale cancel handle fails safely");
    Expect(ClearAnimatedOverride(f.scene, f.node, UIAnimatedProperty::Opacity) &&
        Near(ResolveAnimatedProperties(*f.scene.TryGet(f.node)).opacity, 1.0f),
        "explicit clear restores design value");

    const auto restore = f.animator.Start(f.scene,
        VecAnimation(f.node, UIAnimatedProperty::Position, {10,20}, {50,60}));
    f.animator.Update(f.scene, 0.5);
    Expect(f.animator.Cancel(f.scene, restore.handle, UIAnimationCancelMode::RestoreBase) &&
        Near(ResolveAnimatedProperties(*f.scene.TryGet(f.node)).position, {10,20}),
        "RestoreBase cancel removes the active override");

    bool completion = false, recursiveRejected = false, deferredStarted = false;
    UIAnimationDesc callback = ScalarAnimation(f.node, UIAnimatedProperty::Opacity, 1, 0);
    callback.durationSeconds = 0;
    callback.onCompleted = [&](UIAnimationHandle, UINodeHandle) {
        completion = true;
        recursiveRejected = f.animator.Update(f.scene, 0).reentrantUpdateRejected;
        deferredStarted = f.animator.Start(f.scene,
            ScalarAnimation(f.node, UIAnimatedProperty::Opacity, 0, 1)).success;
    };
    f.animator.Start(f.scene, callback);
    const auto update = f.animator.Update(f.scene, 0);
    Expect(completion && recursiveRejected && deferredStarted && update.hasActiveAnimations,
        "completion callbacks reject recursive Update and defer new starts safely");

    bool canceled = false;
    UIAnimationDesc cancelDesc = VecAnimation(f.node, UIAnimatedProperty::Position, {10,20}, {50,60});
    cancelDesc.onCanceled = [&](UIAnimationHandle, UINodeHandle) { canceled = true; };
    f.animator.Start(f.scene, cancelDesc);
    Expect(f.animator.CancelAllForNode(f.scene, f.node, UIAnimationCancelMode::RestoreBase) >= 1 && canceled,
        "CancelAllForNode releases channels and executes cancel callback");

    bool cancelStartedDeferred = false;
    UIAnimationDesc cancelStarts = VecAnimation(
        f.node, UIAnimatedProperty::Position, {10,20}, {30,40});
    cancelStarts.onCanceled = [&](UIAnimationHandle, UINodeHandle) {
        cancelStartedDeferred = f.animator.Start(f.scene,
            ScalarAnimation(f.node, UIAnimatedProperty::Opacity, 1, 0)).success;
    };
    const UIAnimationHandle cancelStartsHandle = f.animator.Start(f.scene, cancelStarts).handle;
    const bool canceledSynchronously = f.animator.Cancel(
        f.scene, cancelStartsHandle, UIAnimationCancelMode::RestoreBase);
    Expect(canceledSynchronously && cancelStartedDeferred && f.animator.HasActiveAnimations() &&
        f.animator.Update(f.scene, 0).hasActiveAnimations,
        "Cancel callback Start is queued safely and wakes animation scheduling");
}

void TestDirtyClassification() {
    auto check = [](UIAnimatedProperty property, UIAnimationDesc desc,
                    bool transform, bool hitTest, const char* message) {
        Fixture f;
        desc.node = f.node; desc.property = property;
        f.scene.ClearDirty(UIDirtyFlags::All);
        const auto started = f.animator.Start(f.scene, desc);
        f.scene.ClearDirty(UIDirtyFlags::All);
        const auto update = f.animator.Update(f.scene, 0.5);
        const UIDirtyFlags dirty = f.scene.SceneDirtyFlags();
        Expect(started.success && update.visualInvalidated &&
            !HasAny(dirty, UIDirtyFlags::Layout) &&
            HasAny(dirty, UIDirtyFlags::Transform) == transform &&
            HasAny(dirty, UIDirtyFlags::HitTest) == hitTest &&
            update.transformInvalidated == transform &&
            update.hitTestInvalidated == hitTest, message);
    };
    check(UIAnimatedProperty::Position,
        VecAnimation({}, UIAnimatedProperty::Position, {10,20}, {20,30}), true, true,
        "Position dirty is Transform/Visual/HitTest");
    check(UIAnimatedProperty::Scale,
        VecAnimation({}, UIAnimatedProperty::Scale, {1,1}, {2,2}), true, true,
        "Scale dirty is Transform/Visual/HitTest");
    check(UIAnimatedProperty::Rotation,
        ScalarAnimation({}, UIAnimatedProperty::Rotation, 0, 1), true, true,
        "Rotation dirty is Transform/Visual/HitTest");
    check(UIAnimatedProperty::Opacity,
        ScalarAnimation({}, UIAnimatedProperty::Opacity, 1, 0), false, false,
        "Opacity dirty is Visual only");
    UIAnimationDesc color;
    color.from = Color4f{}; color.to = Color4f{0.2f,0.3f,0.4f,0.5f}; color.durationSeconds = 1;
    color.fillMode = UIAnimationFillMode::Forwards;
    check(UIAnimatedProperty::ColorMultiplier, color, false, false, "ColorMultiplier dirty is Visual only");
    UIAnimationDesc visualSize = VecAnimation({}, UIAnimatedProperty::VisualSize, {100,80}, {120,90});
    check(UIAnimatedProperty::VisualSize, visualSize, true, true,
        "VisualSize dirty is Transform/Visual/HitTest and avoids Layout");

    Fixture stable;
    UIAnimationDesc noChange = ScalarAnimation(stable.node, UIAnimatedProperty::Opacity, 1, 1);
    stable.scene.ClearDirty(UIDirtyFlags::All);
    stable.animator.Start(stable.scene, noChange);
    stable.scene.ClearDirty(UIDirtyFlags::All);
    const std::uint64_t revision = stable.scene.VisualRevision();
    stable.animator.Update(stable.scene, 0.5);
    Expect(stable.scene.VisualRevision() == revision,
        "unchanged animation values do not increase visual revision");
}

void TestRuntimeConsistencyAndHidden() {
    BasicLayoutEngine layout;
    BasicUIHitTester hit;
    BasicUIInputRouter router;
    BasicUIRenderBuilder render;
    UIWindowRuntime runtime({91,1}, {layout, hit, router, render}, nullptr);
    const UINodeHandle node = runtime.Scene().CreateNode("runtime-node");
    const UINodeHandle child = runtime.Scene().CreateNode("runtime-child", node);
    runtime.Scene().SetLayout(node, Fixed(100, 80));
    UITransform transform; transform.position = {10,20}; transform.size = {100,80};
    runtime.Scene().SetTransform(node, transform);
    runtime.Scene().SetVisual(node, UIPanelVisual{{1,1,1,1}});
    runtime.Scene().SetLayout(child, Fixed(20, 20));
    runtime.Scene().SetVisual(child, UIPanelVisual{{1,1,1,1}});
    runtime.Update(0, {400,300}, 1);

    auto position = VecAnimation(node, UIAnimatedProperty::Position, {10,20}, {110,20});
    runtime.StartAnimation(position);
    runtime.Update(0.5, {400,300}, 1);
    Expect(runtime.HitTest({140,70}).target == node && !runtime.HitTest({20,70}).hit,
        "position animation updates render transform and hit test in the same frame");
    auto size = VecAnimation(node, UIAnimatedProperty::VisualSize, {100,80}, {200,80});
    runtime.StartAnimation(size); runtime.Update(0.5, {400,300}, 1);
    Expect(runtime.HitTest({200,40}).target == node &&
        Near(ResolveAnimatedProperties(*runtime.Scene().TryGet(node)).visualSize, {150,80}),
        "VisualSize interpolation updates hit geometry without layout measure");

    auto opacity = ScalarAnimation(node, UIAnimatedProperty::Opacity, 1, 0);
    runtime.StartAnimation(opacity); runtime.Update(0.25, {400,300}, 1);
    const float beforeHidden = ResolveAnimatedProperties(*runtime.Scene().TryGet(node)).opacity;
    Expect(Near(runtime.Scene().TryGet(child)->layoutState.effectiveOpacity, 0.75f),
        "animated parent opacity composes into child effectiveOpacity in the same frame");
    runtime.SetVisible(false); runtime.Update(10, {400,300}, 1);
    Expect(Near(ResolveAnimatedProperties(*runtime.Scene().TryGet(node)).opacity, beforeHidden),
        "hidden UIWindowRuntime does not advance animation time");
    runtime.SetVisible(true); runtime.Update(0.25, {400,300}, 1);
    Expect(Near(ResolveAnimatedProperties(*runtime.Scene().TryGet(node)).opacity, 0.5f),
        "visible runtime resumes from pre-hidden progress");
    Expect(runtime.QueryFrameActivity().hasActiveAnimation, "running animation sets UIFrameActivity active");
    runtime.Update(1.0, {400,300}, 1);
    Expect(!runtime.QueryFrameActivity().hasActiveAnimation, "completed animation returns runtime to Static eligibility");

    const UINodeHandle doomed = runtime.Scene().CreateNode("doomed");
    runtime.Scene().SetLayout(doomed, Fixed(20,20));
    bool completed = false;
    UIAnimationDesc active = ScalarAnimation(doomed, UIAnimatedProperty::Opacity, 1, 0);
    active.onCompleted = [&](UIAnimationHandle, UINodeHandle) { completed = true; };
    const UIAnimationHandle activeHandle = runtime.StartAnimation(active).handle;
    UIAnimationDesc queued = active; queued.replaceMode = UIAnimationReplaceMode::Queue;
    const UIAnimationHandle queuedHandle = runtime.StartAnimation(queued).handle;
    runtime.Scene().DestroyNode(doomed);
    Expect(!runtime.CancelAnimation(activeHandle, UIAnimationCancelMode::RestoreBase) &&
        !runtime.CancelAnimation(queuedHandle, UIAnimationCancelMode::RestoreBase) && !completed,
        "node destruction cancels active and queued records without Completion");

    const UINodeHandle callbackNode = runtime.Scene().CreateNode("completion-mutation");
    runtime.Scene().SetLayout(callbackNode, Fixed(20,20));
    UIAnimationDesc callbackMutation = ScalarAnimation(
        callbackNode, UIAnimatedProperty::Opacity, 1, 0);
    callbackMutation.durationSeconds = 0;
    callbackMutation.onCompleted = [&](UIAnimationHandle, UINodeHandle completedNode) {
        runtime.Mutations().Enqueue(DestroyNodeMutation{completedNode});
    };
    runtime.StartAnimation(callbackMutation);
    runtime.Update(0, {400,300}, 1);
    Expect(runtime.Scene().TryGet(callbackNode) == nullptr,
        "Completion callback mutations flush safely after animator traversal");

    const UINodeHandle hiddenNode = runtime.Scene().CreateNode("hidden-animated");
    runtime.Scene().SetLayout(hiddenNode, Fixed(20,20));
    runtime.Update(0, {400,300}, 1);
    runtime.Scene().SetVisibility(hiddenNode, false);
    runtime.Update(0, {400,300}, 1);
    const auto hiddenAnimation = runtime.StartAnimation(
        ScalarAnimation(hiddenNode, UIAnimatedProperty::Opacity, 1, 0));
    const UI2DUpdateResult hiddenUpdate = runtime.Update(0.5, {400,300}, 1);
    Expect(hiddenAnimation.success && runtime.ResumeAnimation(hiddenAnimation.handle) == false &&
        Near(ResolveAnimatedProperties(*runtime.Scene().TryGet(hiddenNode)).opacity, 0.5f) &&
        !hiddenUpdate.drawListRebuilt,
        "single hidden nodes keep advancing without producing render work");
    runtime.Scene().SetEnabled(hiddenNode, false);
    Expect(runtime.QueryFrameActivity().hasActiveAnimation,
        "disabling a node does not cancel its animation");
}

void TestTransformHitGeometry() {
    RuntimeFixture fixture({93,1});
    UINodeHandle node = fixture.AddPanel("geometry", {}, 100,100,100,50, {1,1,1,1});
    UITransform transform = fixture.runtime.Scene().TryGet(node)->transform;
    transform.pivot = {0.5f,0.5f};
    fixture.runtime.Scene().SetTransform(node, transform);
    fixture.runtime.Update(0, {500,400}, 1);

    fixture.runtime.StartAnimation(VecAnimation(node, UIAnimatedProperty::Scale, {1,1}, {2,2}));
    fixture.runtime.Update(0.5, {500,400}, 1);
    Expect(fixture.runtime.HitTest({215,125}).target == node &&
        !fixture.runtime.HitTest({260,125}).hit,
        "Scale animation updates the inverse matrix used by HitTest");
    fixture.runtime.CancelAllAnimations(UIAnimationCancelMode::RestoreBase);
    fixture.runtime.Update(0, {500,400}, 1);

    fixture.runtime.StartAnimation(
        ScalarAnimation(node, UIAnimatedProperty::Rotation, 0, 1.5707963268f));
    fixture.runtime.Update(1.0, {500,400}, 1);
    Expect(fixture.runtime.HitTest({130,80}).target == node &&
        !fixture.runtime.HitTest({105,105}).hit,
        "Rotation animation updates HitTest geometry rather than only drawing");

    UINodeHandle child = fixture.AddPanel("geometry-child", node, 10,10,20,20, {1,0,0,1});
    fixture.runtime.ClearAnimatedOverride(node, UIAnimatedProperty::Rotation);
    fixture.runtime.Update(0, {500,400}, 1);
    fixture.runtime.StartAnimation(VecAnimation(node, UIAnimatedProperty::Scale, {1,1}, {1.5f,1.5f}));
    fixture.runtime.Update(1.0, {500,400}, 1);
    const UINodeRecord* childRecord = fixture.runtime.Scene().TryGet(child);
    const Vec2F childCenter = TransformPoint(
        childRecord->layoutState.computedTransform.localToScene, {10,10});
    Expect(fixture.runtime.HitTest(childCenter).target == child,
        "parent animation composes into child Transform and HitTest");
}

void TestDrawCommandAnimationValues() {
    RuntimeFixture fixture({94,1});
    UINodeHandle node = fixture.AddPanel("draw-values", {}, 20,20,100,60,
        {0.8f,0.6f,0.4f,0.8f});
    fixture.runtime.Update(0, {300,200}, 1);
    fixture.runtime.StartAnimation(ScalarAnimation(node, UIAnimatedProperty::Opacity, 1, 0.5f));
    UIAnimationDesc color;
    color.node = node; color.property = UIAnimatedProperty::ColorMultiplier;
    color.from = Color4f{}; color.to = Color4f{0.5f,1.0f,0.25f,0.5f};
    color.durationSeconds = 1; color.fillMode = UIAnimationFillMode::Forwards;
    fixture.runtime.StartAnimation(color);
    fixture.runtime.Update(0.5, {300,200}, 1);

    const SolidRectCommand* panel = nullptr;
    for (const DrawCommand2D& command : fixture.runtime.DrawList().Commands()) {
        if (const auto* solid = std::get_if<SolidRectCommand>(&command);
            solid && Near(solid->rect.width, 100) && Near(solid->rect.height, 60)) {
            panel = solid;
            break;
        }
    }
    Expect(panel && Near(panel->color.r, 0.27f) && Near(panel->color.g, 0.27f) &&
        Near(panel->color.b, 0.1125f) && Near(panel->color.a, 0.45f),
        "Opacity and ColorMultiplier reach premultiplied DrawCommand color exactly once");
}

void TestReparentAndDpiConsistency() {
    RuntimeFixture reparent({95,1});
    UINodeHandle parentA = reparent.AddPanel("parent-a", {}, 20,40,140,100);
    UINodeHandle parentB = reparent.AddPanel("parent-b", {}, 240,40,140,100);
    UINodeHandle child = reparent.AddPanel("moving-child", parentA, 10,10,40,30, {1,1,1,1});
    reparent.runtime.Update(0, {500,300}, 1);
    auto moving = VecAnimation(child, UIAnimatedProperty::Position, {10,10}, {70,10});
    const auto movingHandle = reparent.runtime.StartAnimation(moving);
    reparent.runtime.Update(0.5, {500,300}, 1);
    const float before = reparent.runtime.Scene().TryGet(child)->layoutState.sceneRect.x;
    reparent.runtime.Scene().ReparentNode(child, parentB);
    reparent.runtime.Update(0, {500,300}, 1);
    const UINodeRecord* moved = reparent.runtime.Scene().TryGet(child);
    const Vec2F center = TransformPoint(moved->layoutState.computedTransform.localToScene, {20,15});
    Expect(movingHandle.success && reparent.runtime.QueryFrameActivity().hasActiveAnimation &&
        moved->layoutState.sceneRect.x > before + 150 && reparent.runtime.HitTest(center).target == child,
        "Reparent keeps animation active and uses the new parent transform in the same frame");

    RuntimeFixture dpiOne({96,1});
    RuntimeFixture dpiTwo({97,1});
    UINodeHandle one = dpiOne.AddPanel("dpi-one", {}, 10,10,50,30);
    UINodeHandle two = dpiTwo.AddPanel("dpi-two", {}, 10,10,50,30);
    dpiOne.runtime.Update(0, {400,300}, 1);
    dpiTwo.runtime.Update(0, {400,300}, 2);
    const UIAnimationStartResult dpiOneAnimation = dpiOne.runtime.StartAnimation(
        VecAnimation(one, UIAnimatedProperty::Position, {10,10}, {110,60}));
    dpiTwo.runtime.StartAnimation(VecAnimation(two, UIAnimatedProperty::Position, {10,10}, {110,60}));
    dpiOne.runtime.Update(0.25, {400,300}, 1);
    dpiTwo.runtime.Update(0.25, {400,300}, 2);
    Expect(Near(ResolveAnimatedProperties(*dpiOne.runtime.Scene().TryGet(one)).position,
                ResolveAnimatedProperties(*dpiTwo.runtime.Scene().TryGet(two)).position) &&
        !dpiTwo.runtime.PauseAnimation(dpiOneAnimation.handle),
        "DPI changes preserve logical values and Runtime animation handles stay isolated");
}

} // namespace

int main() {
    TestMathAndCreation();
    TestCurrentReplaceQueue();
    TestDelayFillLoopDirection();
    TestPauseCancelCallbacks();
    TestDirtyClassification();
    TestRuntimeConsistencyAndHidden();
    TestTransformHitGeometry();
    TestDrawCommandAnimationValues();
    TestReparentAndDpiConsistency();
    if (failures) {
        std::cerr << failures << " animator tests failed\n";
        return 1;
    }
    std::cout << "UI2D animator tests passed\n";
    return 0;
}
