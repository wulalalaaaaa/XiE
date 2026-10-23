#include "UI2D/Widgets/Scrolling/UIScrollModel.h"
#include "UI2D/Widgets/Scrolling/UIScrollPhysics.h"
#include "UI2D/HitTest/BasicUIHitTester.h"
#include "UI2D/Input/BasicUIInputRouter.h"
#include "UI2D/Layout/BasicLayoutEngine.h"
#include "UI2D/Rendering/BasicUIRenderBuilder.h"
#include "UI2D/Runtime/UIWindowRuntime.h"
#include "UI2D/Widgets/Basic/UIButtonWidget.h"
#include "UI2D/Widgets/Basic/UIPanelWidget.h"
#include "UI2D/Widgets/Scrolling/UIScrollViewWidget.h"

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
bool Near(float a, float b, float epsilon = 0.01f) { return std::abs(a - b) <= epsilon; }
UILayoutParams Fixed(float width, float height, UILayoutMode mode = UILayoutMode::Absolute) {
    UILayoutParams result; result.mode = mode;
    result.sizeRule.width = {UISizeMode::Fixed, width};
    result.sizeRule.height = {UISizeMode::Fixed, height};
    return result;
}
UITransform At(float x, float y, float width, float height) {
    UITransform result; result.position = {x,y}; result.size = {width,height}; return result;
}

struct Fixture {
    WindowHandle window{710,1};
    BasicLayoutEngine layout;
    BasicUIHitTester hit;
    BasicUIInputRouter input;
    BasicUIRenderBuilder render;
    UIWindowRuntime runtime;
    Fixture() : runtime(window, {layout,hit,input,render}, {}) {
        (void)runtime.EnqueueInput(WindowFocusEvent{window, true, 0.0});
    }
    void Update(double seconds = 0.0, float dpi = 1.0f) {
        (void)runtime.Update(seconds, {480,360}, dpi);
    }
    void Pointer(Vec2F point, bool down, double timestamp) {
        PointerButtonEvent event; event.window = window; event.logicalPosition = point;
        event.button = PointerButton::Primary; event.pressed = down;
        event.timestampSeconds = timestamp; runtime.EnqueueInput(event);
    }
    void Move(Vec2F point, double timestamp) {
        PointerMoveEvent event; event.window = window; event.logicalPosition = point;
        event.timestampSeconds = timestamp; runtime.EnqueueInput(event);
    }
    void Wheel(Vec2F point, Vec2F delta) {
        PointerWheelEvent event; event.window = window; event.logicalPosition = point;
        event.logicalDelta = delta; runtime.EnqueueInput(event);
    }
};

UIScrollViewWidgetDesc ViewDesc(float x, float y, float width, float height) {
    UIScrollViewWidgetDesc desc;
    desc.common.layout = Fixed(width, height);
    desc.common.transform = At(x, y, width, height);
    desc.contentLayout.mode = UILayoutMode::VerticalStack;
    desc.policy.vertical = UIScrollMode::Auto;
    desc.policy.horizontal = UIScrollMode::Disabled;
    desc.policy.smoothWheel = false;
    desc.policy.enableInertia = false;
    desc.createHorizontalScrollbar = false;
    return desc;
}

UIPanelWidgetDesc Item(float height, Color4f color = {0.3f,0.5f,0.8f,1}) {
    UIPanelWidgetDesc item;
    item.common.layout.sizeRule.width.mode = UISizeMode::Stretch;
    item.common.layout.sizeRule.height = {UISizeMode::Fixed, height};
    item.backgroundColor = color;
    return item;
}

void TestModel() {
    UIScrollModel model;
    UIScrollPolicy policy; policy.horizontal = UIScrollMode::Auto; policy.vertical = UIScrollMode::Auto;
    model.SetPolicy(policy); model.SetViewportSize({100,80}); model.SetContentSize({340,200});
    Expect(model.Offset() == Vec2F{} && model.MaximumOffset() == Vec2F{240,120},
        "ScrollModel starts at zero and computes both maximum offsets");
    Expect(model.SetOffset({400,-20}, UIScrollInputMode::Programmatic) &&
        model.Offset() == Vec2F{240,0}, "SetOffset clamps independently on both axes");
    Expect(model.ScrollBy({-120,50}, UIScrollInputMode::Wheel) && model.Offset() == Vec2F{120,50},
        "ScrollBy applies a bounded delta");
    Expect(model.SetNormalizedOffset({0.5f,1.0f}) && model.Offset() == Vec2F{120,120} &&
        model.NormalizedOffset() == Vec2F{0.5f,1.0f}, "normalized offsets map to the current range");
    const std::uint64_t revision = model.Revision();
    Expect(!model.SetOffset(model.Offset(), UIScrollInputMode::Programmatic) && model.Revision() == revision,
        "unchanged offset does not increment revision");
    model.SetContentSize({40,30});
    Expect(model.MaximumOffset() == Vec2F{} && model.Offset() == Vec2F{},
        "content shrinking below viewport clamps range and offset to zero");
    model.SetViewportSize({std::numeric_limits<float>::quiet_NaN(), -4});
    model.SetContentSize({std::numeric_limits<float>::infinity(), 50});
    Expect(model.ViewportSize() == Vec2F{} && model.ContentSize() == Vec2F{0,50},
        "non-finite and negative sizes normalize safely");
    model.SetViewportSize({100,100}); model.SetContentSize({300,500});
    Expect(model.CanScroll(UIScrollAxis::Horizontal) && model.CanScroll(UIScrollAxis::Vertical) &&
        Near(model.PageStep(UIScrollAxis::Vertical), 90), "axis queries and page steps use logical pixels");
}

void TestPhysics() {
    UIScrollModel model; UIScrollPolicy policy;
    policy.horizontal = UIScrollMode::Auto; policy.vertical = UIScrollMode::Auto;
    policy.inertiaDeceleration = 100; policy.minimumInertiaVelocity = 1;
    model.SetPolicy(policy); model.SetViewportSize({100,100}); model.SetContentSize({500,500});
    UISmoothScrollState smooth; smooth.startOffset = {}; smooth.targetOffset = {100,80};
    smooth.durationSeconds = 0.2; smooth.active = true;
    const auto halfway = UIScrollPhysics::AdvanceSmooth(model, smooth, 0.1);
    Expect(halfway.offsetChanged && smooth.active && model.Offset().x > 50 && model.Offset().x < 100,
        "smooth scrolling advances with the shared easing function");
    const auto finished = UIScrollPhysics::AdvanceSmooth(model, smooth, 0.1);
    Expect(finished.completed && !smooth.active && Near(model.Offset().x,100),
        "smooth scrolling reaches its target and becomes inactive");
    UIScrollInertiaState inertia; inertia.velocity = {200,100}; inertia.active = true;
    const Vec2F before = model.Offset();
    const auto advanced = UIScrollPhysics::AdvanceInertia(model, inertia, policy, 0.1);
    Expect(advanced.offsetChanged && inertia.velocity.x < 200 && model.Offset().x > before.x,
        "inertia advances offset and applies constant per-axis deceleration");
    model.SetOffset(model.MaximumOffset(), UIScrollInputMode::Programmatic);
    inertia.velocity = {200,100}; inertia.active = true;
    const auto boundary = UIScrollPhysics::AdvanceInertia(model, inertia, policy, 0.1);
    Expect(boundary.completed && !inertia.active && inertia.velocity == Vec2F{},
        "inertia stops both blocked axes without overscroll");
}

void TestWidgetLayoutTransformAndLifecycle() {
    Fixture f;
    auto desc = ViewDesc(10, 10, 120, 100);
    const UIWidgetHandle view = f.runtime.Widgets().CreateScrollView(desc);
    const UIWidgetHandle host = f.runtime.Widgets().ContentHost(view);
    const UIWidgetRecord* viewRecord = f.runtime.Widgets().Registry().TryGet(view);
    const UIWidgetRecord* hostRecord = f.runtime.Widgets().Registry().TryGet(host);
    Expect(view.IsValid() && host.IsValid() && viewRecord && hostRecord &&
        viewRecord->ownedNodes.size() == 2, "ScrollView creates Root, Viewport, and a Widget content host");
    Expect(viewRecord && f.runtime.Scene().TryGet(viewRecord->scrollView->viewportNode)->clipChildren &&
        hostRecord && f.runtime.Scene().TryGet(hostRecord->rootNode)->parent == viewRecord->scrollView->viewportNode,
        "Viewport owns the existing Rect clip and mounts ContentHost beneath it");

    UIWidgetHandle items[4]{};
    for (int i = 0; i < 4; ++i) {
        items[i] = f.runtime.Widgets().CreatePanel(Item(50));
        Expect(f.runtime.Widgets().AddContent(view, items[i]), "arbitrary Widget content accepts queued reparent");
    }
    f.Update();
    const UIScrollSnapshot snapshot = f.runtime.Widgets().GetScrollSnapshot(view);
    Expect(snapshot.viewportSize == Vec2F{120,100} && snapshot.contentSize.y >= 200 &&
        Near(snapshot.maximumOffset.y, 100), "layout extent drives viewport, content, and range in logical pixels");
    Expect(f.runtime.Widgets().Registry().TryGet(items[0])->parentWidget == host,
        "AddContent reparents through the existing WidgetRegistry ownership rules");

    int changed = 0, completed = 0;
    f.runtime.Widgets().OnScrollChanged(view, [&](const UIScrollChangedEvent&) { ++changed; });
    f.runtime.Widgets().OnScrollCompleted(view, [&](const UIScrollCompletedEvent&) { ++completed; });
    Expect(f.runtime.Widgets().SetScrollOffset(view, {0,80}, false),
        "immediate programmatic scrolling changes the authoritative model");
    const UINodeHandle hostNode = f.runtime.Widgets().Registry().TryGet(host)->rootNode;
    const Vec2F runtimeOffset = f.runtime.Scene().GetWidgetRuntimeOffset(hostNode);
    Expect(runtimeOffset == Vec2F{0,-80}, "Scroll offset maps to one negative ContentHost runtime offset");
    f.Update();
    Expect(changed == 1 && completed == 1 && !f.runtime.QueryFrameActivity().hasActiveInteraction,
        "changed/completed signals dispatch once and a settled view returns Static");
    const UINodeRecord* third = f.runtime.Scene().TryGet(
        f.runtime.Widgets().Registry().TryGet(items[2])->rootNode);
    Expect(third && third->layoutState.sceneRect.y < 50,
        "the shared final transform applies runtime scroll offset to descendants");
    const UIHitTestResult hit = f.runtime.HitTest({30,35});
    Expect(hit.target == f.runtime.Widgets().Registry().TryGet(items[2])->rootNode,
        "HitTest observes the same scrolled transform and viewport clip as rendering");

    const UINodeHandle oldRoot = f.runtime.Widgets().Registry().TryGet(view)->rootNode;
    Expect(f.runtime.Widgets().Destroy(view), "ScrollView destruction queues through Widget lifecycle");
    f.Update();
    Expect(!f.runtime.Widgets().Registry().TryGet(view) && !f.runtime.Scene().TryGet(oldRoot) &&
        !f.runtime.Widgets().Registry().TryGet(host) && !f.runtime.Widgets().Registry().TryGet(items[0]),
        "destroy cleans internal nodes and content according to current parent-owns-child semantics");
}

void TestWheelNestedAndDragConflict() {
    Fixture f;
    auto outerDesc = ViewDesc(10,10,160,120);
    outerDesc.createVerticalScrollbar = false;
    const UIWidgetHandle outer = f.runtime.Widgets().CreateScrollView(outerDesc);
    auto innerDesc = ViewDesc(0,0,160,100);
    innerDesc.common.transform = At(0,0,160,100);
    innerDesc.createVerticalScrollbar = false;
    const UIWidgetHandle inner = f.runtime.Widgets().CreateScrollView(innerDesc);
    Expect(f.runtime.Widgets().AddContent(outer, inner), "nested ScrollView mounts as ordinary Widget content");
    for (int i = 0; i < 3; ++i) {
        const UIWidgetHandle item = f.runtime.Widgets().CreatePanel(Item(50));
        f.runtime.Widgets().AddContent(inner, item);
    }
    const UIWidgetHandle outerFiller = f.runtime.Widgets().CreatePanel(Item(200));
    f.runtime.Widgets().AddContent(outer, outerFiller);
    f.Update();
    f.runtime.Widgets().SetScrollOffset(inner, {0,40}, false);
    f.Update();
    f.Wheel({40,40}, {0,-1});
    f.Update();
    Expect(Near(f.runtime.Widgets().GetScrollOffset(inner).y, 50) &&
        Near(f.runtime.Widgets().GetScrollOffset(outer).y, 38),
        "inner Wheel consumes only boundary distance and bubbles the remaining delta to its ancestor");

    Fixture buttons;
    auto buttonViewDesc = ViewDesc(10,10,140,100);
    buttonViewDesc.createVerticalScrollbar = false;
    const UIWidgetHandle view = buttons.runtime.Widgets().CreateScrollView(buttonViewDesc);
    UIButtonWidgetDesc buttonDesc;
    buttonDesc.panel.common.layout.sizeRule.width.mode = UISizeMode::Stretch;
    buttonDesc.panel.common.layout.sizeRule.height = {UISizeMode::Fixed, 60};
    buttonDesc.panel.backgroundColor = {0.2f,0.5f,0.9f,1};
    const UIWidgetHandle button = buttons.runtime.Widgets().CreateButton(buttonDesc);
    buttons.runtime.Widgets().AddContent(view, button);
    const UIWidgetHandle filler = buttons.runtime.Widgets().CreatePanel(Item(160));
    buttons.runtime.Widgets().AddContent(view, filler);
    int activations = 0;
    buttons.runtime.Widgets().OnActivated(button,
        [&](const UIButtonActivatedEvent&) { ++activations; });
    buttons.Update();
    buttons.Pointer({40,30}, true, 1.0); buttons.Pointer({40,30}, false, 1.02); buttons.Update();
    Expect(activations == 1, "Button short press below drag threshold still synthesizes Click");
    buttons.Pointer({40,30}, true, 2.0);
    buttons.Move({40,5}, 2.02);
    buttons.Pointer({40,5}, false, 2.04);
    buttons.Update();
    Expect(activations == 1 && buttons.runtime.Widgets().GetScrollOffset(view).y > 0,
        "crossing drag threshold cancels Button Click and scrolls opposite pointer motion");
    Expect(!buttons.runtime.QueryFrameActivity().hasActiveInteraction,
        "PointerUp releases capture and disabled inertia returns Static");
}

void TestSmoothHiddenScrollbarAndBringIntoView() {
    Fixture f;
    auto desc = ViewDesc(10,10,120,100);
    desc.policy.smoothWheel = true;
    desc.policy.smoothDurationSeconds = 0.2;
    desc.minimumThumbLength = 18;
    const UIWidgetHandle view = f.runtime.Widgets().CreateScrollView(desc);
    UIWidgetHandle last{};
    for (int i = 0; i < 4; ++i) {
        UIButtonWidgetDesc button;
        button.panel.common.layout.sizeRule.width.mode = UISizeMode::Stretch;
        button.panel.common.layout.sizeRule.height = {UISizeMode::Fixed, 50};
        button.panel.common.focusable = true; button.panel.common.tabIndex = i;
        last = f.runtime.Widgets().CreateButton(button);
        f.runtime.Widgets().AddContent(view, last);
    }
    f.Update();
    f.Wheel({30,30}, {0,-1}); f.Update(0.05);
    const float beforeHide = f.runtime.Widgets().GetScrollOffset(view).y;
    Expect(beforeHide > 0 && beforeHide < 48 && f.runtime.QueryFrameActivity().hasActiveInteraction,
        "smooth Wheel advances without jumping and keeps frame activity Active");
    f.runtime.SetVisible(false); f.Update(1.0);
    Expect(Near(f.runtime.Widgets().GetScrollOffset(view).y, beforeHide),
        "Hidden runtime pauses smooth scroll without consuming wall-clock time");
    f.runtime.SetVisible(true); f.Update(0.15);
    Expect(Near(f.runtime.Widgets().GetScrollOffset(view).y, 48),
        "show resumes the paused smooth scroll to its clamped target");
    Expect(!f.runtime.Interaction().IsWindowFocused(),
        "show does not invent native window focus");
    (void)f.runtime.EnqueueInput(WindowFocusEvent{f.window, true, 1.2});
    f.Update();

    UIWidgetRecord* record = f.runtime.Widgets().Registry().TryGet(view);
    const UIWidgetHandle scrollbar = record->scrollView->verticalScrollbar;
    UIWidgetRecord* bar = f.runtime.Widgets().Registry().TryGet(scrollbar);
    Expect(bar && bar->scrollbar && Near(bar->scrollbar->trackLength,100) &&
        Near(bar->scrollbar->thumbLength,50) && Near(bar->scrollbar->thumbPosition,24),
        "Scrollbar thumb uses viewport/content ratio and normalized model offset");
    const float beforeThumbDrag = f.runtime.Widgets().GetScrollOffset(view).y;
    f.Pointer({124,50}, true, 3.0); f.Update();
    f.Move({124,75}, 3.02); f.Update();
    f.Pointer({124,75}, false, 3.04); f.Update();
    Expect(f.runtime.Widgets().GetScrollOffset(view).y > beforeThumbDrag &&
        !f.runtime.QueryFrameActivity().hasActiveInteraction,
        "Thumb capture drag updates the bound model immediately and releases without inertia");

    f.runtime.Widgets().SetScrollOffset(view, {0,0}, false); f.Update();
    const UINodeHandle lastNode = f.runtime.Widgets().Registry().TryGet(last)->rootNode;
    f.runtime.RequestFocus(lastNode); f.Update(); f.Update(0.2);
    Expect(f.runtime.FocusedNode() == lastNode,
        "focus auto-scroll does not replace FocusManager authority");
    Expect(f.runtime.Widgets().GetScrollOffset(view).y > 0,
        "FocusGained starts bringing an offscreen descendant into view");
    f.runtime.Widgets().SetScrollOffset(view, {0,0}, false); f.Update();
    Expect(f.runtime.Widgets().ScrollNodeIntoView(
        view, lastNode, UIScrollAlignment::Nearest, false),
        "ScrollNodeIntoView accepts descendants of ContentHost");
    f.Update();
    Expect(Near(f.runtime.Widgets().GetScrollOffset(view).y,100),
        "Nearest bring-into-view performs only the required clamped layout-space movement");
    Expect(!f.runtime.Widgets().ScrollNodeIntoView(
        view, f.runtime.Scene().Root(), UIScrollAlignment::Start, false),
        "ScrollNodeIntoView rejects nodes outside the content subtree safely");
}
}

int main() {
    TestModel(); TestPhysics(); TestWidgetLayoutTransformAndLifecycle();
    TestWheelNestedAndDragConflict(); TestSmoothHiddenScrollbarAndBringIntoView();
    if (failures) { std::cerr << failures << " scroll tests failed\n"; return 1; }
    std::cout << "UI2D scroll model tests passed\n"; return 0;
}
