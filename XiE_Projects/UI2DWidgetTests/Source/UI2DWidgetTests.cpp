#include "UI2D/HitTest/BasicUIHitTester.h"
#include "UI2D/Input/BasicUIInputRouter.h"
#include "UI2D/Layout/BasicLayoutEngine.h"
#include "UI2D/Rendering/BasicUIRenderBuilder.h"
#include "UI2D/Runtime/UIWindowRuntime.h"
#include "UI2D/Theme/BasicUIThemeResolver.h"
#include "UI2D/Widgets/Basic/UIButtonWidget.h"
#include "UI2D/Widgets/Basic/UIImageWidget.h"
#include "UI2D/Widgets/Basic/UIPanelWidget.h"
#include "UI2D/Widgets/Basic/UIProgressBarWidget.h"
#include "UI2D/Widgets/Basic/UITextWidget.h"
#include "UI2D/Widgets/Basic/UIToggleButtonWidget.h"

#include <cmath>
#include <iostream>

namespace {
using namespace Engine;
using namespace Engine::UI2D;
int failures = 0;
void Expect(bool condition, const char* message) {
    std::cout << (condition ? "[PASS] " : "[FAIL] ") << message << '\n';
    if (!condition) ++failures;
}
bool Near(float a, float b) { return std::abs(a - b) < 0.002f; }
UILayoutParams Fixed(float width, float height) {
    UILayoutParams result; result.sizeRule.width = {UISizeMode::Fixed, width};
    result.sizeRule.height = {UISizeMode::Fixed, height}; return result;
}
UITransform At(float x, float y, float width, float height) {
    UITransform result; result.position = {x,y}; result.size = {width,height}; return result;
}
std::shared_ptr<const ResolvedUITheme> WidgetTheme() {
    UIThemeDefinition definition; definition.name = "WidgetTests";
    UIStyleClassDefinition toggle; toggle.name = "Widget.Toggle";
    toggle.baseStyle.opacityMultiplier = 1.0f;
    UIStyleRuleDefinition checked; checked.requiredStates = ToMask(UIInteractionState::Checked);
    checked.properties.opacityMultiplier = 0.5f;
    checked.transitions.opacity = {0.15, UIEasing::EaseOutQuad}; toggle.rules.push_back(checked);
    definition.styleClasses.push_back(toggle);
    return BasicUIThemeResolver{}.Resolve(definition).theme;
}
struct Fixture {
    BasicLayoutEngine layout; BasicUIHitTester hit; BasicUIInputRouter input; BasicUIRenderBuilder render;
    std::shared_ptr<const ResolvedUITheme> theme = WidgetTheme();
    UIWindowRuntime runtime;
    explicit Fixture(WindowHandle window = {601,1}) : runtime(window, {layout,hit,input,render}, theme) {
        (void)runtime.EnqueueInput(WindowFocusEvent{window, true, 0.0});
        Update();
    }
    void Update(double delta = 0) { runtime.Update(delta, {480,320}, 1); }
    void Click(float x, float y) {
        PointerMoveEvent move; move.window = runtime.Window(); move.logicalPosition = {x,y};
        runtime.EnqueueInput(move);
        PointerButtonEvent button; button.window = runtime.Window(); button.logicalPosition = {x,y};
        button.button = PointerButton::Primary; button.pressed = true; runtime.EnqueueInput(button);
        button.pressed = false; runtime.EnqueueInput(button); Update();
    }
    void DownUp(Vec2F down, Vec2F up) {
        PointerButtonEvent button; button.window = runtime.Window();
        button.button = PointerButton::Primary; button.logicalPosition = down;
        button.pressed = true; (void)runtime.EnqueueInput(button); Update();
        button.logicalPosition = up; button.pressed = false;
        (void)runtime.EnqueueInput(button); Update();
    }
    void FocusWindow(bool focused) {
        (void)runtime.EnqueueInput(
            WindowFocusEvent{runtime.Window(), focused, 0.0});
        Update();
    }
    void Key(InputKeyCode key, bool pressed, bool repeat = false) {
        KeyEvent event; event.window = runtime.Window(); event.key = key; event.pressed = pressed; event.repeat = repeat;
        runtime.EnqueueInput(event); Update();
    }
};
UIPanelWidgetDesc PanelDesc(float x=10, float y=10, float w=300, float h=240) {
    UIPanelWidgetDesc desc; desc.common.layout = Fixed(w,h); desc.common.transform = At(x,y,w,h);
    desc.backgroundColor = {0.1f,0.12f,0.16f,1}; desc.cornerRadius = 8; return desc;
}
UIButtonWidgetDesc ButtonDesc(float x, float y);

void TestCompositeButtonPointerOwnership() {
    Fixture f;
    const UIWidgetHandle outer = f.runtime.Widgets().CreateButton(
        ButtonDesc(20,20));
    UITextWidgetDesc text; text.common.layout = Fixed(70,30);
    text.common.transform = At(30,10,70,30); text.textLayout = {71,1};
    (void)f.runtime.Widgets().CreateText(text, outer);
    UIImageWidgetDesc icon; icon.common.layout = Fixed(20,20);
    icon.common.transform = At(5,15,20,20); icon.texture = {72,1};
    (void)f.runtime.Widgets().CreateImage(icon, outer);
    int outerActivations = 0;
    (void)f.runtime.Widgets().OnActivated(
        outer, [&](const auto&) { ++outerActivations; });
    f.Update();

    // Screen-space button bounds are [20,140]x[20,70].
    f.DownUp({55,40}, {23,40});
    f.DownUp({23,40}, {55,40});
    f.DownUp({30,40}, {60,40});
    f.DownUp({55,40}, {58,41});
    Expect(outerActivations == 4,
        "composite Button clicks across Text, Image and background leaves");

    f.DownUp({55,40}, {180,40});
    Expect(outerActivations == 4,
        "release outside the interaction owner cancels Click");

    const UIWidgetHandle adjacent = f.runtime.Widgets().CreateButton(
        ButtonDesc(160,20));
    int adjacentActivations = 0;
    (void)f.runtime.Widgets().OnActivated(
        adjacent, [&](const auto&) { ++adjacentActivations; });
    f.Update();
    f.DownUp({55,40}, {170,40});
    Expect(outerActivations == 4 && adjacentActivations == 0,
        "Down on one Button and Up on another activates neither");

    UIButtonWidgetDesc innerDesc = ButtonDesc(20,10);
    innerDesc.panel.common.layout = Fixed(60,30);
    innerDesc.panel.common.transform = At(20,10,60,30);
    const UIWidgetHandle inner =
        f.runtime.Widgets().CreateButton(innerDesc, outer);
    int innerActivations = 0;
    (void)f.runtime.Widgets().OnActivated(
        inner, [&](const auto&) { ++innerActivations; });
    f.Update();
    f.DownUp({45,35}, {47,36});
    Expect(innerActivations == 1 && outerActivations == 4,
        "nearest nested interaction owner activates without outer Button");

    f.DownUp({125,65}, {125,65});
    Expect(outerActivations == 5,
        "outer Button remains independently clickable outside nested owner");

    PointerButtonEvent down{f.runtime.Window(), {125,65},
        PointerButton::Primary, true};
    (void)f.runtime.EnqueueInput(down); f.Update();
    (void)f.runtime.Widgets().SetEnabled(outer, false); f.Update();
    down.pressed = false; (void)f.runtime.EnqueueInput(down); f.Update();
    Expect(outerActivations == 5,
        "disabling the interaction owner during press cancels Click");

    (void)f.runtime.Widgets().SetEnabled(outer, true); f.Update();
    down.pressed = true; (void)f.runtime.EnqueueInput(down); f.Update();
    f.FocusWindow(false);
    down.pressed = false; (void)f.runtime.EnqueueInput(down); f.Update();
    Expect(outerActivations == 5,
        "window focus loss cancels composite Button press exactly once");

    f.FocusWindow(true);
    down.pressed = true; (void)f.runtime.EnqueueInput(down); f.Update();
    (void)f.runtime.Widgets().SetVisible(outer, false); f.Update();
    down.pressed = false; (void)f.runtime.EnqueueInput(down); f.Update();
    Expect(outerActivations == 5,
        "hiding the interaction owner during press cancels Click");

    Fixture destroyed;
    const UIWidgetHandle doomed = destroyed.runtime.Widgets().CreateButton(
        ButtonDesc(20,20));
    int destroyedActivations = 0;
    (void)destroyed.runtime.Widgets().OnActivated(
        doomed, [&](const auto&) { ++destroyedActivations; });
    destroyed.Update();
    PointerButtonEvent doomedPointer{destroyed.runtime.Window(), {30,30},
        PointerButton::Primary, true};
    (void)destroyed.runtime.EnqueueInput(doomedPointer); destroyed.Update();
    (void)destroyed.runtime.Widgets().Destroy(doomed); destroyed.Update();
    doomedPointer.pressed = false;
    (void)destroyed.runtime.EnqueueInput(doomedPointer); destroyed.Update();
    Expect(destroyedActivations == 0,
        "destroying the interaction owner during press cancels Click");
}
UIButtonWidgetDesc ButtonDesc(float x=20, float y=20) {
    UIButtonWidgetDesc desc; desc.panel.common.layout = Fixed(120,50);
    desc.panel.common.transform = At(x,y,120,50); desc.panel.backgroundColor = {0.2f,0.5f,0.9f,1};
    return desc;
}

void TestHandlesOwnershipAndProperties() {
    Fixture a({601,1}), b({602,1});
    auto context = a.runtime.Widgets().Context();
    const UIWidgetHandle panel = UIPanelWidget::Create(context, PanelDesc());
    UIPanelWidgetDesc nineDesc = PanelDesc(330,10,100,80); nineDesc.nineSliceTexture = {12,1};
    nineDesc.clipChildren = true;
    const UIWidgetHandle nine = a.runtime.Widgets().CreatePanel(nineDesc);
    UIImageWidgetDesc imageDesc; imageDesc.common.layout = Fixed(40,40); imageDesc.common.transform = At(5,5,40,40);
    imageDesc.texture = {4,1}; imageDesc.fit = UIImageFit::Contain;
    const UIWidgetHandle image = a.runtime.Widgets().CreateImage(imageDesc, panel);
    UITextWidgetDesc textDesc; textDesc.common.layout = Fixed(80,20); textDesc.common.transform = At(50,5,80,20);
    textDesc.textLayout = {7,1};
    const UIWidgetHandle text = a.runtime.Widgets().CreateText(textDesc, panel);
    Expect(panel.IsValid() && image.IsValid() && text.IsValid() && nine.IsValid() &&
        a.runtime.Widgets().Registry().Size() == 4,
        "widget creation returns valid handles and records ownership");
    const UIWidgetRecord* panelRecord = a.runtime.Widgets().Registry().TryGet(panel);
    const UIWidgetRecord* imageRecord = a.runtime.Widgets().Registry().TryGet(image);
    Expect(panelRecord && imageRecord && imageRecord->parentWidget == panel &&
        a.runtime.Scene().TryGet(imageRecord->rootNode)->parent == panelRecord->rootNode,
        "widget parent relation mounts the Scene root predictably");
    Expect(!b.runtime.Widgets().Registry().TryGet(panel), "widget handles cannot cross window runtimes");
    const std::size_t liveNodes = a.runtime.Scene().LiveNodes().size();
    Expect(!a.runtime.Widgets().CreatePanel(PanelDesc(), UIWidgetHandle{9999,9999}).IsValid() &&
        a.runtime.Scene().LiveNodes().size() == liveNodes,
        "invalid parent construction fails without leaking Scene nodes");
    const UIWidgetRecord* nineRecord = a.runtime.Widgets().Registry().TryGet(nine);
    Expect(nineRecord && std::holds_alternative<UINineSliceVisual>(
        a.runtime.Scene().TryGet(nineRecord->rootNode)->visual) &&
        a.runtime.Scene().TryGet(nineRecord->rootNode)->clipChildren,
        "Panel selects NineSlice visual and preserves clipping semantics");

    Expect(a.runtime.Widgets().SetTexture(image, {9,2}) &&
        a.runtime.Widgets().SetImageFit(image, UIImageFit::Cover) &&
        a.runtime.Widgets().SetTint(image, {0.5f,0.6f,0.7f,1}),
        "image properties update the existing UIImageVisual");
    Expect(a.runtime.Widgets().SetTextLayout(text, {11,3}) &&
        a.runtime.Widgets().SetTextColor(text, {0.9f,0.8f,0.7f,1}),
        "text properties update a caller-owned TextLayout handle and color");
    Expect(a.runtime.Widgets().SetVisible(image, false) && a.runtime.Widgets().SetEnabled(text, false),
        "common visible and enabled mutations are accepted");
    const UILayoutParams resized = Fixed(72,48); const UITransform moved = At(8,9,72,48);
    Expect(a.runtime.Widgets().SetFocusable(image, true) && a.runtime.Widgets().SetTabIndex(image, 7) &&
        a.runtime.Widgets().SetLayout(image, resized) && a.runtime.Widgets().SetTransform(image, moved) &&
        a.runtime.Widgets().SetStyleClass(image, a.theme->FindStyleClass("Widget.Toggle")),
        "focus, tab, layout, transform, and style common properties update the root node");
    a.Update();
    const UINodeRecord* imageNode = a.runtime.Scene().TryGet(imageRecord->rootNode);
    Expect(!imageNode->visible && imageNode->focus.focusable && imageNode->focus.tabIndex == 7 &&
        imageNode->layout == resized && imageNode->transform == moved &&
        imageNode->style.reference.styleClass == a.theme->FindStyleClass("Widget.Toggle") &&
        !a.runtime.Scene().TryGet(a.runtime.Widgets().Registry().TryGet(text)->rootNode)->enabled,
        "common property mutations reach widget root nodes at the safe point");

    const UINodeHandle oldRoot = panelRecord->rootNode;
    Expect(a.runtime.Widgets().Destroy(panel) && !a.runtime.Widgets().Destroy(panel),
        "duplicate destroy requests are rejected safely");
    a.Update();
    Expect(!a.runtime.Widgets().Registry().TryGet(panel) && !a.runtime.Widgets().Registry().TryGet(image) &&
        !a.runtime.Scene().TryGet(oldRoot), "destroying a widget subtree destroys only its owned widget nodes");
    const UIWidgetHandle reused = a.runtime.Widgets().CreatePanel(PanelDesc());
    Expect(reused.index == panel.index && reused.generation != panel.generation,
        "reused widget slots receive a new generation");
}

void TestConnectionsButtonAndMutationSafety() {
    Fixture f;
    const UIWidgetHandle panel = f.runtime.Widgets().CreatePanel(PanelDesc());
    const UIWidgetHandle button = f.runtime.Widgets().CreateButton(ButtonDesc(), panel);
    UITextWidgetDesc content; content.common.layout = Fixed(100,30);
    content.common.transform = At(5,5,100,30); content.textLayout = {44,1};
    const UIWidgetHandle buttonContent = f.runtime.Widgets().CreateText(content, button);
    const UIWidgetRecord* buttonRecord = f.runtime.Widgets().Registry().TryGet(button);
    const UINodeHandle buttonRoot = buttonRecord->rootNode;
    int activations = 0; UIButtonActivatedEvent::Source last{};
    const UIWidgetConnectionHandle connection = f.runtime.Widgets().OnActivated(button,
        [&](const UIButtonActivatedEvent& event) { ++activations; last = event.source; });
    f.Update(); f.Click(45,45);
    Expect(activations == 1 && last == UIButtonActivatedEvent::Source::Pointer &&
        f.runtime.IsFocused(buttonRoot), "pointer Click activates and focuses a Button through existing input semantics");
    Expect(buttonContent.IsValid(), "Button child content does not mask bubbled Click activation");
    f.Key(InputKeyCode::Enter, true); f.Key(InputKeyCode::Enter, true, true);
    Expect(activations == 2 && last == UIButtonActivatedEvent::Source::KeyboardEnter,
        "Enter activates once and key repeat is suppressed");
    f.Key(InputKeyCode::Space, true);
    Expect(HasInteractionState(f.runtime.Scene().GetExplicitInteractionStates(buttonRecord->rootNode),
        UIInteractionState::Pressed), "Space KeyDown exposes pressed semantics without replacing InputRouter state");
    f.Key(InputKeyCode::Space, false);
    Expect(activations == 3 && last == UIButtonActivatedEvent::Source::KeyboardSpace,
        "Space activates on KeyUp and clears explicit pressed state");
    Expect(f.runtime.Widgets().Activate(button) && activations == 4 &&
        last == UIButtonActivatedEvent::Source::Programmatic, "programmatic activation reports its source");
    bool recursiveRejected = false;
    const UIWidgetConnectionHandle recursion = f.runtime.Widgets().OnActivated(button,
        [&](const UIButtonActivatedEvent&) { recursiveRejected = !f.runtime.Widgets().Activate(button); });
    Expect(f.runtime.Widgets().Activate(button) && recursiveRejected,
        "recursive Activate is rejected while the current activation is dispatching");
    f.runtime.Widgets().Disconnect(recursion);
    Expect(f.runtime.Widgets().Disconnect(connection) && !f.runtime.Widgets().Disconnect(connection),
        "connection handles disconnect once and then become stale");

    f.runtime.Widgets().SetEnabled(button, false); f.Update(); f.Click(45,45);
    Expect(activations == 5, "disabled Button rejects user activation");

    const UIWidgetHandle doomed = f.runtime.Widgets().CreateButton(ButtonDesc(160,20), panel);
    bool callbackFinished = false; bool connectionRejectedWhileDestroyPending = false;
    const UIWidgetConnectionHandle doomedConnection = f.runtime.Widgets().OnActivated(doomed, [&](const UIButtonActivatedEvent&) {
        Expect(f.runtime.Widgets().Destroy(doomed), "destroy requested inside callback is queued");
        connectionRejectedWhileDestroyPending = !f.runtime.Widgets().OnActivated(
            doomed, [](const UIButtonActivatedEvent&) {}).IsValid();
        callbackFinished = true;
    });
    f.Update(); f.Click(185,45);
    Expect(callbackFinished && !f.runtime.Widgets().Registry().TryGet(doomed),
        "callback destruction flushes after listener dispatch and before render");
    Expect(!f.runtime.Widgets().Disconnect(doomedConnection),
        "Widget destruction automatically invalidates all connection handles");
    Expect(connectionRejectedWhileDestroyPending,
        "a Widget pending destruction cannot register new connections");

    UIButtonWidgetDesc noFocusDesc = ButtonDesc(160,100);
    noFocusDesc.behavior.focusOnPointerDown = false;
    const UIWidgetHandle noFocus = f.runtime.Widgets().CreateButton(noFocusDesc, panel);
    f.runtime.Widgets().SetEnabled(button, true); f.Update();
    f.runtime.RequestFocus(buttonRoot); f.Update(); f.Click(185,125);
    Expect(noFocus.IsValid() && f.runtime.FocusedNode() == buttonRoot,
        "focusOnPointerDown=false preserves the current focus while retaining Click behavior");
}

void TestToggleCheckedTheme() {
    Fixture f;
    UIToggleButtonWidgetDesc desc; desc.button = ButtonDesc();
    desc.button.panel.common.styleClass = f.theme->FindStyleClass("Widget.Toggle");
    const UIWidgetHandle toggle = f.runtime.Widgets().CreateToggleButton(desc);
    int changes = 0; bool last = false;
    f.runtime.Widgets().OnCheckedChanged(toggle, [&](bool value) { ++changes; last = value; });
    f.Update(); f.Click(45,45); f.Update();
    UIWidgetRecord* record = f.runtime.Widgets().Registry().TryGet(toggle);
    Expect(f.runtime.Widgets().IsChecked(toggle) && changes == 1 && last &&
        HasInteractionState(f.runtime.Scene().GetExplicitInteractionStates(record->rootNode), UIInteractionState::Checked),
        "pointer activation toggles Checked and stores it as an explicit style state");
    Expect(f.runtime.QueryFrameActivity().hasActiveAnimation,
        "Checked style changes enter the existing interaction transition channel");
    f.Update(0.2);
    Expect(Near(f.runtime.TryGetRuntimeStyle(record->rootNode)->opacityMultiplier, 0.5f),
        "Checked participates in normal Theme selector resolution");
    Expect(f.runtime.Widgets().SetChecked(toggle, false, false) && changes == 1,
        "programmatic SetChecked can suppress the changed event");
    record = f.runtime.Widgets().Registry().TryGet(toggle); record->allowUncheck = false;
    f.runtime.Widgets().SetChecked(toggle, true, false); f.Click(45,45);
    Expect(f.runtime.Widgets().IsChecked(toggle), "allowUncheck=false prevents user activation from clearing Checked");
    record->allowUncheck = true; f.runtime.Widgets().SetChecked(toggle, false, false);
    f.runtime.Widgets().SetEnabled(toggle, false); f.Update();
    f.runtime.RequestFocus(record->rootNode); f.Update(); f.Key(InputKeyCode::Space, true); f.Key(InputKeyCode::Space, false);
    Expect(!f.runtime.Widgets().IsChecked(toggle), "disabled Toggle rejects keyboard state changes");
    f.runtime.Widgets().SetEnabled(toggle, true); f.Update();
    f.runtime.RequestFocus(record->rootNode); f.Update();
    f.Key(InputKeyCode::Space, true); f.Key(InputKeyCode::Space, false);
    Expect(f.runtime.Widgets().IsChecked(toggle), "focused Toggle uses Button keyboard activation semantics");
}

void TestProgressAndReparent() {
    Fixture f;
    const UIWidgetHandle left = f.runtime.Widgets().CreatePanel(PanelDesc(10,10,200,180));
    const UIWidgetHandle right = f.runtime.Widgets().CreatePanel(PanelDesc(250,10,200,180));
    UIProgressBarWidgetDesc desc; desc.common.layout = Fixed(160,24);
    desc.common.transform = At(10,20,160,24); desc.value = 0.25f;
    const UIWidgetHandle progress = f.runtime.Widgets().CreateProgressBar(desc, left);
    f.Update();
    const UIWidgetRecord* record = f.runtime.Widgets().Registry().TryGet(progress);
    Expect(record && record->ownedNodes.size() == 3 && !f.runtime.Scene().TryGet(record->rootNode)->focus.focusable,
        "ProgressBar owns root/background/fill and is not focusable");
    Expect(Near(f.runtime.Scene().TryGet(record->progressFill)->layout.anchor.maxX, 0.25f),
        "left-to-right fill geometry uses normalized anchor extent");
    f.runtime.Widgets().SetValue(progress, 2.0f);
    Expect(Near(f.runtime.Widgets().Value(progress), 1.0f), "ProgressBar value clamps to its range");
    f.runtime.Widgets().SetValue(progress, 0.25f);
    f.runtime.Widgets().SetProgressDirection(progress, UIProgressDirection::RightToLeft);
    Expect(Near(f.runtime.Scene().TryGet(record->progressFill)->layout.anchor.minX, 0.75f),
        "right-to-left progress anchors the fill at the right edge");
    f.runtime.Widgets().SetProgressDirection(progress, UIProgressDirection::BottomToTop);
    Expect(Near(f.runtime.Scene().TryGet(record->progressFill)->layout.anchor.minY, 0.75f),
        "bottom-to-top progress anchors the fill at the bottom edge");
    f.runtime.Widgets().SetProgressDirection(progress, UIProgressDirection::TopToBottom);
    Expect(Near(f.runtime.Scene().TryGet(record->progressFill)->layout.anchor.maxY, 0.25f),
        "top-to-bottom progress anchors the fill at the top edge");
    Expect(!f.runtime.Widgets().SetRange(progress, 1,1), "invalid progress range is rejected explicitly");
    Expect(f.runtime.Widgets().Reparent(progress, right), "widget reparent mutation is accepted");
    f.Update(); record = f.runtime.Widgets().Registry().TryGet(progress);
    Expect(record->parentWidget == right && f.runtime.Scene().TryGet(record->rootNode)->parent ==
        f.runtime.Widgets().Registry().TryGet(right)->rootNode,
        "reparent updates Widget and Scene relationships in the same frame");
    Expect(f.runtime.Widgets().Reparent(right, progress), "cycle request reaches the safe mutation point");
    const UI2DUpdateResult cycle = f.runtime.Update(0, {480,320}, 1);
    Expect(cycle.widgets.rejectedMutationCount == 1, "widget parent cycles are rejected without damaging either tree");
}

void TestStaticHiddenAndExternalInvalidation() {
    Fixture f;
    const UIWidgetHandle panel = f.runtime.Widgets().CreatePanel(PanelDesc());
    f.Update();
    const UI2DUpdateResult stable = f.runtime.Update(0, {480,320}, 1);
    Expect(stable.widgets.appliedMutationCount == 0 && !stable.drawListRebuilt,
        "empty Widget mutation queue takes the static fast path");
    f.runtime.SetVisible(false); f.runtime.Widgets().SetVisible(panel, false);
    const UI2DUpdateResult hidden = f.runtime.Update(0, {480,320}, 1);
    Expect(hidden.widgets.appliedMutationCount == 0 && f.runtime.Widgets().HasPendingMutations(),
        "hidden window does not flush Widget mutations");
    f.runtime.SetVisible(true); f.Update();
    const UIWidgetRecord* record = f.runtime.Widgets().Registry().TryGet(panel);
    Expect(record && !f.runtime.Scene().TryGet(record->rootNode)->visible,
        "pending Widget mutation is applied after the window is shown");
    const UINodeHandle root = record->rootNode;
    f.runtime.Scene().DestroyNode(root); f.Update();
    Expect(!f.runtime.Widgets().Registry().TryGet(panel),
        "external node invalidation cleans the owning Widget record and connections");
}
} // namespace

int main() {
    TestHandlesOwnershipAndProperties();
    TestConnectionsButtonAndMutationSafety();
    TestCompositeButtonPointerOwnership();
    TestToggleCheckedTheme();
    TestProgressAndReparent();
    TestStaticHiddenAndExternalInvalidation();
    if (failures) { std::cerr << failures << " widget tests failed\n"; return 1; }
    std::cout << "UI2D widget tests passed\n"; return 0;
}
