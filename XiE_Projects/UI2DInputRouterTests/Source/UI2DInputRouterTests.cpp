#include "Input/InputEventQueue.h"
#include "Renderer2D/TextureRegistry2D.h"
#include "UI2D/Animation/IUIAnimator.h"
#include "UI2D/HitTest/BasicUIHitTester.h"
#include "UI2D/Input/BasicUIInputRouter.h"
#include "UI2D/Input/BasicUIFocusManager.h"
#include "UI2D/Input/UIFocusRequestQueue.h"
#include "UI2D/Input/UIEventListenerRegistry.h"
#include "UI2D/Input/UIInteractionState.h"
#include "UI2D/Layout/BasicLayoutEngine.h"
#include "UI2D/Rendering/BasicUIRenderBuilder.h"
#include "UI2D/Runtime/UIWindowRuntime.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {
using namespace Engine;
using namespace Engine::UI2D;

int failures = 0;
void Expect(bool condition, const char* message) {
    std::cout << (condition ? "[PASS] " : "[FAIL] ") << message << '\n';
    if (!condition) ++failures;
}

UILayoutParams Fixed(float width, float height) {
    UILayoutParams value;
    value.sizeRule.width = {UISizeMode::Fixed, width};
    value.sizeRule.height = {UISizeMode::Fixed, height};
    return value;
}
void Place(UIScene& scene, UINodeHandle node, float x, float y, float width, float height) {
    scene.SetLayout(node, Fixed(width, height));
    UITransform transform = scene.TryGet(node)->transform;
    transform.position = {x, y};
    scene.SetTransform(node, transform);
}

struct Fixture {
    WindowHandle window{1,1};
    UIScene scene;
    BasicLayoutEngine layout;
    BasicUIHitTester hit;
    BasicUIFocusManager focus;
    UIFocusRequestQueue focusRequests;
    UIEventListenerRegistry listeners;
    UISceneMutationQueue mutations;
    UIInteractionRuntimeState interaction;
    BasicUIInputRouter router;
    UINodeHandle parent{};
    UINodeHandle target{};
    UINodeHandle sibling{};

    Fixture() {
        scene.SetNodeInvalidatedCallback([this](UINodeHandle node){
            listeners.RemoveAllForNode(node);
            interaction.OnNodeInvalidated(node);
            router.OnNodeInvalidated(scene,node);
        });
        parent = scene.CreateNode("Parent"); Place(scene,parent,10,10,160,100);
        target = scene.CreateNode("Target",parent); Place(scene,target,20,20,50,40);
        sibling = scene.CreateNode("Sibling",parent); Place(scene,sibling,90,20,50,40);
        scene.SetFocusable(target, true);
        Relayout();
        UIFocusDispatchContext context{
            window,listeners,mutations,interaction.capture,focusRequests,{}};
        focus.OnWindowFocusChanged(scene, true, context);
    }
    void Relayout(Vec2F size={240,160}, float dpi=1.0f) {
        static ResolvedUITheme theme;
        const auto result = layout.UpdateLayout(scene,{size,dpi,theme,scene.Root()});
        if (result.Succeeded()) scene.ClearDirty(UIDirtyFlags::Layout);
    }
    UIInputDispatchResult Dispatch(std::span<const InputEvent> events, UIClickPolicy policy={}) {
        UIInputDispatchContext context{
            window,hit,focus,listeners,mutations,interaction,policy,focusRequests,{}};
        return router.Dispatch(scene,events,context);
    }
    UIInputDispatchResult Dispatch(const InputEvent& event, UIClickPolicy policy={}) {
        return Dispatch(std::span<const InputEvent>(&event,1),policy);
    }
    void Focus(UINodeHandle node) {
        focusRequests.Enqueue({node,UIFocusReason::Programmatic,UIFocusRequestPriority::Explicit,false});
        UIFocusDispatchContext context{window,listeners,mutations,interaction.capture,focusRequests,{}};
        (void)focusRequests.Flush(scene,focus,context);
    }
    void ClearFocus() {
        focusRequests.Enqueue({{},UIFocusReason::Clear,UIFocusRequestPriority::Explicit,true});
        UIFocusDispatchContext context{window,listeners,mutations,interaction.capture,focusRequests,{}};
        (void)focusRequests.Flush(scene,focus,context);
    }
};

PointerMoveEvent Move(WindowHandle w, Vec2F p, PointerId id=0) {
    PointerMoveEvent value; value.window=w; value.logicalPosition=p; value.pointerId=id; return value;
}
PointerButtonEvent Button(WindowHandle w, Vec2F p, bool down,
    PointerButton button=PointerButton::Left, PointerId id=0) {
    PointerButtonEvent value; value.window=w; value.logicalPosition=p; value.pressed=down;
    value.button=button; value.pointerId=id; return value;
}

void TestRoutingAndPropagation() {
    Fixture f;
    std::vector<std::string> log;
    auto add = [&](UINodeHandle node, UIEventPhase phase, const char* name) {
        UIEventPhaseMask mask = phase==UIEventPhase::Preview ? UIEventPhaseMask::Preview
            : phase==UIEventPhase::Target ? UIEventPhaseMask::Target : UIEventPhaseMask::Bubble;
        f.listeners.AddListener(node,UIEventType::PointerMove,mask,[&,name,node,phase](UIEventContext& c){
            const UIEvent& e=c.Event();
            log.push_back(name);
            Expect(e.currentTarget==node&&e.phase==phase,"4. currentTarget and phase update per listener");
            if(node==f.target) Expect(std::abs(e.localPosition.x-5)<0.01f&&std::abs(e.localPosition.y-5)<0.01f,
                "5. currentTarget receives its own localPosition");
        });
    };
    add(f.scene.Root(),UIEventPhase::Preview,"preview-root");
    add(f.parent,UIEventPhase::Preview,"preview-parent");
    add(f.target,UIEventPhase::Preview,"preview-target");
    add(f.target,UIEventPhase::Target,"target");
    add(f.target,UIEventPhase::Bubble,"bubble-target");
    add(f.parent,UIEventPhase::Bubble,"bubble-parent");
    add(f.scene.Root(),UIEventPhase::Bubble,"bubble-root");
    const InputEvent move=Move(f.window,{35,35}); const auto result=f.Dispatch(move);
    const std::vector<std::string> expected={"preview-root","preview-parent","preview-target","target","bubble-target","bubble-parent","bubble-root"};
    Expect(log==expected,"1-3. Preview, Target and Bubble use stable Root/Target order");
    Expect(result.hitTestCount==1&&result.dispatchedEventCount>=1,"6-7. Valid route dispatches and no-target paths remain safe");

    Fixture handled;
    bool bubbleObserved=false;
    handled.listeners.AddListener(handled.scene.Root(),UIEventType::PointerDown,UIEventPhaseMask::Preview,
        [](UIEventContext& c){c.MarkHandled();});
    handled.listeners.AddListener(handled.parent,UIEventType::PointerDown,UIEventPhaseMask::Bubble,
        [&](UIEventContext& c){bubbleObserved=c.Event().handled;});
    const InputEvent down=Button(handled.window,{35,35},true); handled.Dispatch(down);
    Expect(bubbleObserved,"8. handled does not stop propagation");

    Fixture prevented;
    prevented.listeners.AddListener(prevented.target,UIEventType::PointerDown,UIEventPhaseMask::Target,
        [](UIEventContext& c){c.PreventDefault();});
    prevented.Dispatch(InputEvent{Button(prevented.window,{35,35},true)});
    Expect(!prevented.interaction.capture.HasCapture()&&!prevented.interaction.press.HasActivePress()&&
        prevented.focus.DebugStats().requests==0,"9-11. PreventDefault blocks capture, press and focus");

    Fixture stopped; int afterStop=0;
    stopped.listeners.AddListener(stopped.parent,UIEventType::PointerMove,UIEventPhaseMask::Preview,
        [](UIEventContext& c){c.StopPropagation();});
    stopped.listeners.AddListener(stopped.target,UIEventType::PointerMove,UIEventPhaseMask::Preview,
        [&](UIEventContext&){++afterStop;});
    stopped.Dispatch(InputEvent{Move(stopped.window,{35,35})});
    Expect(afterStop==0,"12. StopPropagation stops subsequent route nodes");

    Fixture immediate; int first=0,second=0;
    immediate.listeners.AddListener(immediate.target,UIEventType::PointerMove,UIEventPhaseMask::Target,
        [&](UIEventContext& c){++first;c.StopImmediatePropagation();});
    immediate.listeners.AddListener(immediate.target,UIEventType::PointerMove,UIEventPhaseMask::Target,
        [&](UIEventContext&){++second;});
    immediate.Dispatch(InputEvent{Move(immediate.window,{35,35})});
    Expect(first==1&&second==0,"13-14. Immediate stop halts remaining stable-order listeners");
}

void TestHoverCapturePressClick() {
    Fixture f;
    int enters=0,leaves=0,moves=0,ups=0,clicks=0;
    f.listeners.AddListener(f.target,UIEventType::PointerEnter,UIEventPhaseMask::Target,[&](UIEventContext&){++enters;});
    f.listeners.AddListener(f.target,UIEventType::PointerLeave,UIEventPhaseMask::Target,[&](UIEventContext&){++leaves;});
    f.listeners.AddListener(f.target,UIEventType::PointerMove,UIEventPhaseMask::Target,[&](UIEventContext&){++moves;});
    f.listeners.AddListener(f.target,UIEventType::PointerUp,UIEventPhaseMask::Target,[&](UIEventContext&){++ups;});
    f.listeners.AddListener(f.target,UIEventType::Click,UIEventPhaseMask::Target,[&](UIEventContext&){++clicks;});
    f.Dispatch(InputEvent{Move(f.window,{35,35})});
    f.Dispatch(InputEvent{Move(f.window,{36,36})});
    Expect(enters==1&&f.interaction.hover.IsHovered(f.target),"15-16. Hover enters once and remains stable inside node");
    Expect(f.scene.HasDirty(UIDirtyFlags::Visual),"24. Hover transition marks Visual dirty");
    f.Dispatch(InputEvent{Move(f.window,{105,35})});
    Expect(leaves==1&&f.interaction.hover.IsHovered(f.sibling),"17-20. Sibling transition diffs common ancestor with correct leave/enter");
    PointerLeaveEvent leave;leave.window=f.window;f.Dispatch(InputEvent{leave});
    Expect(!f.interaction.hover.IsHovered(f.sibling),"21. PointerLeave clears complete Hover route");

    f.Dispatch(InputEvent{Button(f.window,{35,35},true)});
    Expect(f.interaction.capture.GetCapturedNode(0)==f.target&&f.interaction.press.IsPressed(f.target),
        "25/37-38. Primary Down establishes Capture and Press");
    f.Dispatch(InputEvent{Move(f.window,{220,140})});
    Expect(!f.interaction.hover.IsHovered(f.target),"22. Capture does not replace physical Hover target");
    f.Dispatch(InputEvent{Button(f.window,{220,140},false)});
    Expect(moves>=1&&ups==1&&!f.interaction.capture.HasCapture()&&!f.interaction.press.HasActivePress(),
        "28-30/39. Captured Move/Up route to pressed node and Up releases state");
    Expect(clicks==0,"46/51. Release outside physical target does not click");

    f.Dispatch(InputEvent{Button(f.window,{35,35},true)});
    f.Dispatch(InputEvent{Button(f.window,{35,35},false)});
    Expect(clicks==1,"45/53-54. Same-node Down/Up produces one Click after PointerUp");

    f.Dispatch(InputEvent{Button(f.window,{35,35},true)});
    f.Dispatch(InputEvent{Move(f.window,{60,35})});
    f.Dispatch(InputEvent{Button(f.window,{35,35},false)});
    Expect(clicks==1,"47-48. Movement threshold cancels Click");

    Fixture explicitCapture;
    explicitCapture.listeners.AddListener(explicitCapture.target,UIEventType::PointerDown,UIEventPhaseMask::Target,
        [&](UIEventContext& c){Expect(c.RequestPointerCapture(0,explicitCapture.sibling),"27. Explicit Capture request succeeds");});
    explicitCapture.Dispatch(InputEvent{Button(explicitCapture.window,{35,35},true)});
    Expect(explicitCapture.interaction.capture.GetCapturedNode(0)==explicitCapture.sibling,
        "27. Explicit Capture overrides default target capture");

    Fixture twoPointers;
    twoPointers.Dispatch(InputEvent{Button(twoPointers.window,{35,35},true,PointerButton::Left,1)});
    twoPointers.Dispatch(InputEvent{Button(twoPointers.window,{105,35},true,PointerButton::Left,2)});
    Expect(twoPointers.interaction.capture.GetCapturedNode(1)==twoPointers.target&&
        twoPointers.interaction.capture.GetCapturedNode(2)==twoPointers.sibling,
        "35. PointerId Capture state is independent");
    twoPointers.Dispatch(InputEvent{PointerCancelEvent{twoPointers.window,1,{35,35},0}});
    Expect(!twoPointers.interaction.capture.GetCapturedNode(1).IsValid()&&
        twoPointers.interaction.capture.GetCapturedNode(2).IsValid(),"31/40. PointerCancel only clears matching Capture and Press");
}

void TestInvalidationAndLatestRoutes() {
    Fixture hoverDestroyed;
    int oldListener=0;
    hoverDestroyed.listeners.AddListener(hoverDestroyed.target,UIEventType::PointerMove,UIEventPhaseMask::Target,
        [&](UIEventContext&){++oldListener;});
    hoverDestroyed.Dispatch(InputEvent{Move(hoverDestroyed.window,{35,35})});
    const UINodeHandle dead=hoverDestroyed.target;
    hoverDestroyed.scene.DestroyNode(dead);
    Expect(!hoverDestroyed.interaction.hover.IsHovered(dead),"23. Destroyed node is removed from Hover routes");
    const auto replacement=hoverDestroyed.scene.CreateNode("Replacement",hoverDestroyed.parent);
    Place(hoverDestroyed.scene,replacement,20,20,50,40);hoverDestroyed.Relayout();
    hoverDestroyed.Dispatch(InputEvent{Move(hoverDestroyed.window,{35,35})});
    Expect(oldListener==1,"67. Destroying node removes all of its listeners");

    Fixture invalidState;
    invalidState.Dispatch(InputEvent{Button(invalidState.window,{35,35},true,PointerButton::Left)});
    invalidState.Dispatch(InputEvent{Button(invalidState.window,{35,35},true,PointerButton::Right)});
    Expect(invalidState.interaction.press.IsPressed(invalidState.target,PointerButton::Left)&&
        invalidState.interaction.press.IsPressed(invalidState.target,PointerButton::Right),
        "44. Multiple button Press records are independent");
    invalidState.scene.SetEnabled(invalidState.target,false);
    const auto released=invalidState.interaction.capture.Sanitize(invalidState.scene);
    const auto canceled=invalidState.interaction.press.Sanitize(invalidState.scene);
    Expect(!released.empty()&&!canceled.empty()&&!invalidState.interaction.capture.HasCapture()&&
        !invalidState.interaction.press.HasActivePress(),"34/43. Disabled node releases Capture and cancels Press");

    Fixture destroyPress;int destroyedClicks=0;
    destroyPress.listeners.AddListener(destroyPress.target,UIEventType::Click,UIEventPhaseMask::Target,
        [&](UIEventContext&){++destroyedClicks;});
    destroyPress.Dispatch(InputEvent{Button(destroyPress.window,{35,35},true)});
    destroyPress.scene.DestroyNode(destroyPress.target);
    destroyPress.Dispatch(InputEvent{Button(destroyPress.window,{35,35},false)});
    Expect(!destroyPress.interaction.capture.HasCapture()&&!destroyPress.interaction.press.HasActivePress()&&destroyedClicks==0,
        "33/42/50. Destroyed node clears Capture/Press and cannot Click");

    Fixture reparent;const auto newParent=reparent.scene.CreateNode("NewParent");
    Place(reparent.scene,newParent,0,0,100,100);reparent.scene.SetHitTestVisible(newParent,false);reparent.Relayout();
    int newParentClicks=0;
    reparent.listeners.AddListener(newParent,UIEventType::Click,UIEventPhaseMask::Bubble,
        [&](UIEventContext&){++newParentClicks;});
    reparent.Dispatch(InputEvent{Button(reparent.window,{35,35},true)});
    reparent.scene.ReparentNode(reparent.target,newParent);reparent.Relayout();
    const auto physicalAfterReparent=reparent.hit.HitTest(reparent.scene,{}, {35,35});
    const auto reparentUp=reparent.Dispatch(InputEvent{Button(reparent.window,{35,35},false)});
    Expect(physicalAfterReparent.target==reparent.target,"66. Reparented node remains the physical target");
    Expect(reparentUp.clickCount==1,"52. Reparented Press synthesizes Click from current parent chain");
    Expect(newParentClicks==1,"52/66. Reparented node uses its latest route for the next event and Click");
}

void TestWheelKeyboardLifecycle() {
    Fixture f;
    std::vector<std::string> wheelRoute;
    for(auto [node,name]:std::vector<std::pair<UINodeHandle,const char*>>{{f.scene.Root(),"root"},{f.parent,"parent"},{f.target,"target"}}){
        f.listeners.AddListener(node,UIEventType::PointerWheel,UIEventPhaseMask::Bubble,
            [&,name](UIEventContext&){wheelRoute.push_back(name);});
    }
    PointerWheelEvent wheel;wheel.window=f.window;wheel.logicalPosition={35,35};wheel.logicalDelta={0,2};
    f.Dispatch(InputEvent{wheel});
    Expect(wheelRoute==std::vector<std::string>({"target","parent","root"}),
        "55/58. Wheel uses physical target and routed phases");
    f.Dispatch(InputEvent{Button(f.window,{105,35},true)});
    wheelRoute.clear();f.Dispatch(InputEvent{wheel});
    Expect(!wheelRoute.empty()&&wheelRoute.front()=="target","56. Capture does not change Wheel physical target");

    int rootWheel=0;
    f.listeners.AddListener(f.scene.Root(),UIEventType::PointerWheel,UIEventPhaseMask::Target,
        [&](UIEventContext&){++rootWheel;});
    wheel.logicalPosition={230,150};f.Dispatch(InputEvent{wheel});
    Expect(rootWheel==1,"57. Wheel without physical target routes to Root");

    int keyDown=0,keyUp=0,text=0;bool repeated=false;
    f.Focus(f.target);
    f.listeners.AddListener(f.target,UIEventType::KeyDown,UIEventPhaseMask::Target,[&](UIEventContext& c){++keyDown;repeated=c.Event().repeat;});
    f.listeners.AddListener(f.target,UIEventType::KeyUp,UIEventPhaseMask::Target,[&](UIEventContext&){++keyUp;});
    f.listeners.AddListener(f.target,UIEventType::TextInput,UIEventPhaseMask::Target,[&](UIEventContext& c){if(c.Event().codepoint==U'\u4e2d')++text;});
    KeyEvent kd;kd.window=f.window;kd.key=InputKeyCode::Enter;kd.pressed=true;kd.repeat=true;
    KeyEvent ku=kd;ku.pressed=false;ku.repeat=false;
    TextInputEvent ti;ti.window=f.window;ti.codepoint=U'\u4e2d';
    const InputEvent keyboard[]={kd,ku,ti};f.Dispatch(keyboard);
    Expect(keyDown==1&&keyUp==1&&text==1&&repeated,"59-61/63. Focused node receives KeyDown, KeyUp, TextInput and repeat");
    f.ClearFocus();int rootKey=0;
    f.listeners.AddListener(f.scene.Root(),UIEventType::KeyDown,UIEventPhaseMask::Target,[&](UIEventContext&){++rootKey;});
    f.Dispatch(InputEvent{kd});Expect(rootKey==1,"62. Keyboard falls back to Root without Focus");

    Fixture lost;int cancels=0;
    lost.listeners.AddListener(lost.target,UIEventType::PointerCancel,UIEventPhaseMask::Target,[&](UIEventContext&){++cancels;});
    lost.Dispatch(InputEvent{Button(lost.window,{35,35},true)});
    WindowFocusEvent focusLost;focusLost.window=lost.window;focusLost.focused=false;lost.Dispatch(InputEvent{focusLost});
    Expect(cancels==1&&!lost.interaction.capture.HasCapture()&&!lost.interaction.press.HasActivePress()&&!lost.focus.HasActiveFocus(),
        "32/41/64. Focus Lost emits Cancel and clears Capture, Press and Focus");
}

void TestListenersMutationAndRuntime() {
    Fixture f;
    int first=0,removed=0;
    UIEventListenerHandle second;
    f.listeners.AddListener(f.target,UIEventType::PointerMove,UIEventPhaseMask::Target,[&](UIEventContext&){++first;f.listeners.RemoveListener(second);});
    second=f.listeners.AddListener(f.target,UIEventType::PointerMove,UIEventPhaseMask::Target,[&](UIEventContext&){++removed;});
    f.Dispatch(InputEvent{Move(f.window,{35,35})});
    Expect(first==1&&removed==1,"69. Listener removal is deferred until current Dispatch completes");
    const auto replacement=f.listeners.AddListener(f.target,UIEventType::Click,UIEventPhaseMask::Target,[](UIEventContext&){});
    Expect(!f.listeners.RemoveListener(second)&&replacement.generation!=second.generation,
        "68. Listener generation rejects stale handles");

    bool existedDuringCallback=false;
    f.listeners.AddListener(f.target,UIEventType::PointerDown,UIEventPhaseMask::Target,[&](UIEventContext& c){
        c.EnqueueMutation(DestroyNodeMutation{f.target});existedDuringCallback=f.scene.TryGet(f.target)!=nullptr;
    });
    f.Dispatch(InputEvent{Button(f.window,{35,35},true)});
    Expect(existedDuringCallback&&f.scene.TryGet(f.target)!=nullptr&&!f.mutations.Empty(),
        "70/72. Callback queues structural Mutation without changing active route");
    f.mutations.Flush(f.scene);
    Expect(!f.scene.TryGet(f.target),"72. Mutation executes only after Dispatch");

    Fixture recursive;bool rejected=false;
    recursive.listeners.AddListener(recursive.target,UIEventType::PointerMove,UIEventPhaseMask::Target,[&](UIEventContext&){
        UIInputDispatchContext c{
            recursive.window,recursive.hit,recursive.focus,recursive.listeners,recursive.mutations,
            recursive.interaction,{},recursive.focusRequests,{}};
        const InputEvent nested=Move(recursive.window,{35,35});
        rejected=recursive.router.Dispatch(recursive.scene,std::span<const InputEvent>(&nested,1),c).reentrantDispatchRejected;
    });
    recursive.Dispatch(InputEvent{Move(recursive.window,{35,35})});
    Expect(rejected,"71. Router explicitly rejects recursive Dispatch");

    BasicLayoutEngine layout;BasicUIHitTester hit;BasicUIInputRouter router;
    NullUIAnimator animator;BasicUIRenderBuilder builder;
    UIWindowRuntime runtime({9,1},{layout,hit,router,builder},nullptr);
    const auto node=runtime.Scene().CreateNode("Interactive");Place(runtime.Scene(),node,0,0,50,50);
    runtime.Scene().SetVisual(node,UIPanelVisual{{1,1,1,1}});
    runtime.Update(0,{100,100},1);
    runtime.AddEventListener(node,UIEventType::PointerMove,UIEventPhaseMask::Target,[](UIEventContext&){});
    runtime.EnqueueInput(Move({9,1},{10,10}));
    Expect(runtime.QueryFrameActivity().hasPendingInput,"73. Pending Input wakes a Static window");
    const auto hover=runtime.Update(0,{100,100},1);
    Expect(hover.inputDispatch.hoverTransitionCount>0&&hover.drawListRebuilt,
        "74. Hover transition invalidates and rebuilds visual DrawList");
    runtime.EnqueueInput(Button({9,1},{10,10},true));const auto press=runtime.Update(0,{100,100},1);
    Expect(press.drawListRebuilt&&press.activity.hasActiveInteraction,
        "75/77. Press triggers rebuild and active interaction scheduling");
    runtime.EnqueueInput(Button({9,1},{10,10},false));runtime.Update(0,{100,100},1);
    Expect(!runtime.QueryFrameActivity().hasActiveInteraction,"76. Completed input returns window to Static");
    runtime.SetVisible(false);
    Expect(!runtime.QueryFrameActivity().hasActiveInteraction&&!runtime.EnqueueInput(Move({9,1},{10,10})),
        "65/78. Hidden runtime clears interaction and rejects queued Move");
}

void TestTransformClipAndWindowIsolation() {
    Fixture first;Fixture second;second.window={2,1};
    first.Dispatch(InputEvent{Button(first.window,{35,35},true)});
    Expect(first.interaction.capture.HasCapture()&&!second.interaction.capture.HasCapture(),
        "36. Capture state is isolated between windows");

    Fixture transformed;
    UITransform tr=transformed.scene.TryGet(transformed.target)->transform;
    tr.pivot={0.5f,0.5f};tr.rotationRadians=0.4f;transformed.scene.SetTransform(transformed.target,tr);
    transformed.Relayout({240,160},2.0f);
    const Vec2F center=TransformPoint(
        transformed.scene.TryGet(transformed.target)->layoutState.computedTransform.localToScene,{25,20});
    int hits=0;
    transformed.listeners.AddListener(transformed.target,UIEventType::PointerMove,UIEventPhaseMask::Target,[&](UIEventContext&){++hits;});
    transformed.Dispatch(InputEvent{Move(transformed.window,center)});
    Expect(hits==1,"81-82. Logical coordinates and rotated transforms target identically at non-unit DPI");

    Fixture clipped;clipped.scene.SetClipChildren(clipped.parent,true);
    Place(clipped.scene,clipped.target,140,20,50,40);clipped.Relayout();
    int clippedHits=0;
    clipped.listeners.AddListener(clipped.target,UIEventType::PointerMove,UIEventPhaseMask::Target,
        [&](UIEventContext&){++clippedHits;});
    clipped.Dispatch(InputEvent{Move(clipped.window,{180,35})});
    clipped.Dispatch(InputEvent{Move(clipped.window,{160,35})});
    Expect(clippedHits==1,"83. Clip outside region never receives Pointer events");
}

void TestInputQueueOverflowReliability() {
    const WindowHandle window{77,1};
    InputEventQueue moves(window, 4);
    for (int i = 0; i < 1000; ++i) {
        PointerMoveEvent move;
        move.window = window;
        move.logicalPosition = {
            static_cast<float>(i), static_cast<float>(i + 1)};
        (void)moves.Push(move);
    }
    const auto coalesced = moves.Drain();
    const auto* finalMove = coalesced.size() == 1
        ? std::get_if<PointerMoveEvent>(&coalesced.front()) : nullptr;
    Expect(finalMove && finalMove->logicalPosition == Vec2F{999,1000} &&
        moves.Stats().coalescedPointerMoves == 999,
        "consecutive PointerMove pressure retains the final coordinates");

    InputEventQueue separatedMoves(window, 4);
    (void)separatedMoves.Push(Button(window, {1,1}, true));
    (void)separatedMoves.Push(Move(window, {2,2}));
    (void)separatedMoves.Push(KeyEvent{window, InputKeyCode::A, true});
    (void)separatedMoves.Push(KeyEvent{window, InputKeyCode::A, false});
    for (int i = 3; i < 500; ++i)
        (void)separatedMoves.Push(Move(window,
            {static_cast<float>(i), static_cast<float>(i + 1)}));
    const auto separated = separatedMoves.Drain();
    const auto* separatedFinal = separated.empty() ? nullptr
        : std::get_if<PointerMoveEvent>(&separated.back());
    Expect(separated.size() == 4 && separatedFinal &&
        separatedFinal->logicalPosition == Vec2F{499,500} &&
        std::holds_alternative<PointerButtonEvent>(separated[0]) &&
        std::holds_alternative<KeyEvent>(separated[1]) &&
        std::holds_alternative<KeyEvent>(separated[2]),
        "non-adjacent PointerMove pressure keeps the latest position after critical events");

    InputEventQueue mixed(window, 4);
    (void)mixed.Push(Button(window, {10,10}, true));
    (void)mixed.Push(Move(window, {11,11}));
    KeyEvent key{window, InputKeyCode::A, true};
    (void)mixed.Push(key);
    (void)mixed.Push(Move(window, {12,12}));
    (void)mixed.Push(TextCompositionCommitEvent{window, "input", 0.0});
    (void)mixed.Push(WindowFocusEvent{window, false, 0.0});
    (void)mixed.Push(Button(window, {12,12}, false));
    const auto events = mixed.Drain();
    std::vector<std::size_t> critical;
    for (std::size_t i = 0; i < events.size(); ++i) {
        if (!std::holds_alternative<PointerMoveEvent>(events[i]))
            critical.push_back(i);
    }
    Expect(critical.size() == 5 &&
        std::holds_alternative<PointerButtonEvent>(events[critical[0]]) &&
        std::get<PointerButtonEvent>(events[critical[0]]).pressed &&
        std::holds_alternative<KeyEvent>(events[critical[1]]) &&
        std::holds_alternative<TextCompositionCommitEvent>(events[critical[2]]) &&
        std::holds_alternative<WindowFocusEvent>(events[critical[3]]) &&
        std::holds_alternative<PointerButtonEvent>(events[critical[4]]) &&
        !std::get<PointerButtonEvent>(events[critical[4]]).pressed,
        "overflow preserves Down, Key, IME Commit, FocusLost and Up order");
    const auto mixedStats = mixed.Stats();
    Expect(mixedStats.evictedPointerMoves > 0 &&
        mixedStats.softCapacityGrowths > 0,
        "overflow is observable and grows only when no coalescible event remains");

    InputEventQueue keys(window, 2);
    for (int i = 0; i < 100; ++i) {
        KeyEvent down{window, InputKeyCode::A, true};
        KeyEvent up{window, InputKeyCode::A, false};
        (void)keys.Push(down);
        (void)keys.Push(up);
    }
    const auto keyEvents = keys.Drain();
    Expect(keyEvents.size() == 200 &&
        keys.Stats().softCapacityGrowths == 198,
        "non-coalescible keyboard state transitions are never evicted");

    InputEventQueue movingKeys(window, 4);
    for (int i = 0; i < 100; ++i) {
        (void)movingKeys.Push(Move(window,
            {static_cast<float>(i), static_cast<float>(i)}));
        (void)movingKeys.Push(KeyEvent{window, InputKeyCode::A, true});
        (void)movingKeys.Push(KeyEvent{window, InputKeyCode::A, false});
    }
    const auto movingKeyEvents = movingKeys.Drain();
    std::size_t keyCount = 0;
    for (const auto& event : movingKeyEvents)
        if (std::holds_alternative<KeyEvent>(event)) ++keyCount;
    Expect(keyCount == 200,
        "high-frequency PointerMove never displaces keyboard transitions");
}

} // namespace

int main() {
    std::cout<<std::unitbuf;
    TestRoutingAndPropagation();
    TestHoverCapturePressClick();
    TestInvalidationAndLatestRoutes();
    TestWheelKeyboardLifecycle();
    TestListenersMutationAndRuntime();
    TestTransformClipAndWindowIsolation();
    TestInputQueueOverflowReliability();
    std::cout<<(failures?"UI2D input-router tests failed\n":"UI2D input-router tests passed\n");
    return failures?EXIT_FAILURE:EXIT_SUCCESS;
}
