#include "UI2D/Input/BasicUIInputRouter.h"

#include "Foundation/Math/Matrix3.h"
#include "UI2D/Core/UIScene.h"
#include "UI2D/Input/IUIFocusManager.h"
#include "UI2D/Input/IUIHitTester.h"
#include "UI2D/Input/UIEventListenerRegistry.h"
#include "UI2D/Input/UIFocusRequestQueue.h"
#include "UI2D/Input/UIFocusUtils.h"
#include "UI2D/Input/UIInteractionState.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <type_traits>

namespace Engine::UI2D {
namespace {

constexpr Engine::PointerButton kPointerButtons[] = {
    Engine::PointerButton::Left,
    Engine::PointerButton::Right,
    Engine::PointerButton::Middle,
    Engine::PointerButton::X1,
    Engine::PointerButton::X2};

bool Eligible(const UIScene& scene, UINodeHandle handle) {
    const UINodeRecord* node = scene.TryGet(handle);
    return node && node->visible && node->enabled && node->layoutState.effectiveVisible &&
        node->layoutState.effectiveEnabled;
}

float DistanceSquared(Engine::Vec2F lhs, Engine::Vec2F rhs) {
    const float x = lhs.x - rhs.x;
    const float y = lhs.y - rhs.y;
    return x * x + y * y;
}

} // namespace

UIHitTestResult BasicUIInputRouter::HitTest(
    const UIScene& scene,
    Engine::Vec2F position,
    UIInputDispatchContext& context,
    UIInputDispatchResult& result) {
    ++result.hitTestCount;
    ++m_LastStats.hitTests;
    return context.hitTester.HitTest(scene, {}, position);
}

bool BasicUIInputRouter::BuildRoute(
    const UIScene& scene, UINodeHandle target, std::vector<UINodeHandle>& route) const {
    route.clear();
    UINodeHandle current = target;
    while (current.IsValid()) {
        const UINodeRecord* node = scene.TryGet(current);
        if (!node) { route.clear(); return false; }
        route.push_back(current);
        if (current == scene.Root()) {
            std::reverse(route.begin(), route.end());
            return true;
        }
        current = node->parent;
    }
    route.clear();
    return false;
}

BasicUIInputRouter::RoutedEventResult BasicUIInputRouter::RouteEvent(
    UIScene& scene,
    UIEvent event,
    std::span<const UINodeHandle> route,
    UIInputDispatchContext& context,
    UIInputDispatchResult& result) {
    RoutedEventResult routed{event, {}};
    if (route.empty() || !scene.TryGet(route.back())) return routed;
    routed.event.target = route.back();
    const UINodeHandle captureBefore = context.interaction.capture.GetCapturedNode(event.pointerId);
    if (m_DebugLoggingEnabled) {
        const UINodeRecord* targetNode = scene.TryGet(route.back());
        AppendDebug("[Route] target=" + (targetNode ? targetNode->debugName : std::string{"<invalid>"}));
    }

    auto invoke = [&](UINodeHandle current, UIEventPhase phase) {
        const UINodeRecord* node = scene.TryGet(current);
        if (!node || !node->layoutState.computedTransform.inverseValid) return;
        routed.event.phase = phase;
        routed.event.currentTarget = current;
        routed.event.localPosition = Engine::TransformPoint(
            node->layoutState.computedTransform.sceneToLocal, routed.event.scenePosition);
        UIEventContext eventContext(
            routed.event, scene, context.focusRequests, context.interaction.capture,
            context.mutationQueue, routed.requests, &context.interaction.press);
        m_LastStats.listenerInvocations += context.listeners.Invoke(
            current, routed.event.type, phase, eventContext);
    };

    for (UINodeHandle node : route) {
        invoke(node, UIEventPhase::Preview);
        if (routed.event.propagationStopped) break;
    }
    if (!routed.event.propagationStopped) invoke(route.back(), UIEventPhase::Target);
    if (!routed.event.propagationStopped) {
        for (std::size_t i = route.size(); i > 0; --i) {
            invoke(route[i - 1], UIEventPhase::Bubble);
            if (routed.event.propagationStopped) break;
        }
    }

    ++result.dispatchedEventCount;
    ++m_LastStats.routedEvents;
    if (routed.requests.mutationQueued) result.mutationQueued = true;
    if (routed.requests.interactionStateChanged) {
        result.interactionStateChanged = true;
        result.visualInvalidated = true;
        context.interaction.Touch();
    }
    const UINodeHandle captureAfter = context.interaction.capture.GetCapturedNode(event.pointerId);
    if (captureAfter != captureBefore) {
        if (captureAfter.IsValid()) ++m_LastStats.capturesStarted;
        if (captureBefore.IsValid()) ++m_LastStats.capturesReleased;
        result.interactionStateChanged = true;
        if (captureBefore.IsValid()) scene.MarkStyleDirty(captureBefore);
        if (captureAfter.IsValid()) scene.MarkStyleDirty(captureAfter);
        context.interaction.Touch();
    }
    return routed;
}

void BasicUIInputRouter::DispatchDirect(
    UIScene& scene,
    UINodeHandle node,
    UIEvent event,
    UIInputDispatchContext& context,
    UIInputDispatchResult& result) {
    const UINodeRecord* record = scene.TryGet(node);
    if (!record || !record->layoutState.computedTransform.inverseValid) return;
    event.target = node;
    event.currentTarget = node;
    event.phase = UIEventPhase::Target;
    event.localPosition = Engine::TransformPoint(
        record->layoutState.computedTransform.sceneToLocal, event.scenePosition);
    UIEventDispatchRequests requests;
    UIEventContext eventContext(
        event, scene, context.focusRequests, context.interaction.capture,
        context.mutationQueue, requests, &context.interaction.press);
    m_LastStats.listenerInvocations += context.listeners.Invoke(
        node, event.type, UIEventPhase::Target, eventContext);
    if (requests.mutationQueued) result.mutationQueued = true;
    if (requests.interactionStateChanged) {
        result.interactionStateChanged = true;
        result.visualInvalidated = true;
        context.interaction.Touch();
    }
    ++result.dispatchedEventCount;
    ++m_LastStats.routedEvents;
}

void BasicUIInputRouter::MarkVisual(
    UIScene& scene, UINodeHandle node, UIInputDispatchResult& result) {
    if (!scene.TryGet(node)) return;
    scene.MarkDirty(node, UIDirtyFlags::Visual);
    scene.MarkStyleDirty(node);
    // MarkVisual is only called for an actual hover/press interaction transition.
    // Capture changes dispatched from listeners are handled in RouteEvent above.
    result.visualInvalidated = true;
    result.interactionStateChanged = true;
}

void BasicUIInputRouter::DispatchHoverTransition(
    UIScene& scene,
    Engine::PointerId pointerId,
    const UIHoverTransition& transition,
    const UIEvent& source,
    UIInputDispatchContext& context,
    UIInputDispatchResult& result) {
    for (UINodeHandle node : transition.leaves) {
        UIEvent leave = source;
        leave.type = UIEventType::PointerLeave;
        leave.pointerId = pointerId;
        DispatchDirect(scene, node, leave, context, result);
        MarkVisual(scene, node, result);
        ++m_LastStats.hoverLeaves;
    }
    for (UINodeHandle node : transition.enters) {
        UIEvent enter = source;
        enter.type = UIEventType::PointerEnter;
        enter.pointerId = pointerId;
        DispatchDirect(scene, node, enter, context, result);
        MarkVisual(scene, node, result);
        ++m_LastStats.hoverEnters;
    }
    const auto count = static_cast<std::uint32_t>(transition.leaves.size() + transition.enters.size());
    result.hoverTransitionCount += count;
}

void BasicUIInputRouter::UpdateHover(
    UIScene& scene,
    Engine::PointerId pointerId,
    const UIHitTestResult& hit,
    const UIEvent& source,
    UIInputDispatchContext& context,
    UIInputDispatchResult& result) {
    const UIHoverTransition transition = context.interaction.hover.Update(pointerId, hit);
    if (transition.Changed()) DispatchHoverTransition(
        scene, pointerId, transition, source, context, result);
}

UINodeHandle BasicUIInputRouter::ResolveKeyboardTarget(
    UIScene& scene, IUIFocusManager& focus) const {
    const UINodeHandle focused = focus.GetFocusedNode();
    UINodeHandle root = focus.GetFocusRoot();
    if (!scene.TryGet(root)) root = scene.Root();
    return focus.HasActiveFocus() && IsFocusable(scene, focused, root) ? focused : scene.Root();
}

void BasicUIInputRouter::HandleFocusLost(
    UIScene& scene,
    const Engine::WindowFocusEvent& input,
    UIInputDispatchContext& context,
    UIInputDispatchResult& result) {
    UIEvent focusEvent;
    focusEvent.type = UIEventType::WindowFocusLost;
    focusEvent.window = input.window;
    focusEvent.timestampSeconds = input.timestampSeconds;
    std::vector<UINodeHandle> focusRoute;
    (void)BuildRoute(scene, ResolveKeyboardTarget(scene, context.focusManager), focusRoute);
    (void)RouteEvent(scene, focusEvent, focusRoute, context, result);

    std::vector<Engine::PointerId> pointers = context.interaction.capture.CapturedPointers();
    for (Engine::PointerId id : context.interaction.press.ActivePointers()) {
        if (std::find(pointers.begin(), pointers.end(), id) == pointers.end()) pointers.push_back(id);
    }
    for (Engine::PointerId pointerId : pointers) {
        UINodeHandle target = context.interaction.capture.GetCapturedNode(pointerId);
        if (!target.IsValid()) {
            for (Engine::PointerButton button : kPointerButtons) {
                if (const UIPressRecord* press = context.interaction.press.TryGet({pointerId, button})) {
                    target = press->pressedNode;
                    break;
                }
            }
        }
        std::vector<UINodeHandle> route;
        if (BuildRoute(scene, target, route)) {
            UIEvent cancel;
            cancel.type = UIEventType::PointerCancel;
            cancel.window = input.window;
            cancel.pointerId = pointerId;
            cancel.timestampSeconds = input.timestampSeconds;
            (void)RouteEvent(scene, cancel, route, context, result);
        }
    }
    for (Engine::PointerId hoverPointer : context.interaction.hover.Pointers()) {
        UIEvent source;
        source.window = input.window;
        source.pointerId = hoverPointer;
        source.timestampSeconds = input.timestampSeconds;
        DispatchHoverTransition(
            scene, hoverPointer, context.interaction.hover.Clear(hoverPointer), source, context, result);
    }
    for (Engine::PointerId pointerId : pointers) {
        if (context.interaction.capture.Release(pointerId)) ++m_LastStats.capturesReleased;
        for (Engine::PointerButton button : kPointerButtons) {
            if (UIPressRecord* press = context.interaction.press.TryGet({pointerId, button})) {
                if (!press->canceled) ++m_LastStats.pressesCanceled;
                MarkVisual(scene, press->pressedNode, result);
            }
        }
        context.interaction.press.CancelPointer(pointerId);
    }
    context.interaction.press.ClearAll();
    UIFocusDispatchContext focusContext{
        context.window, context.listeners, context.mutationQueue, context.interaction.capture,
        context.focusRequests, context.focusPolicy};
    context.focusManager.OnWindowFocusChanged(scene, false, focusContext);
    result.interactionStateChanged = true;
}

UIInputDispatchResult BasicUIInputRouter::Dispatch(
    UIScene& scene,
    std::span<const Engine::InputEvent> events,
    UIInputDispatchContext& context) {
    UIInputDispatchResult result;
    if (m_DispatchInProgress) {
        result.reentrantDispatchRejected = true;
        return result;
    }
    m_DispatchInProgress = true;
    m_ActiveContext = &context;
    m_LastStats = {};
    m_LastDebugLog.clear();
    for (const Engine::InputEvent& input : events) {
        if (Engine::GetInputEventWindow(input) != context.window) continue;
        context.listeners.BeginDispatch();
        const std::uint64_t visualBeforeFocus = scene.VisualRevision();
        UIFocusDispatchContext focusContext{
            context.window, context.listeners, context.mutationQueue, context.interaction.capture,
            context.focusRequests, context.focusPolicy};
        // Requests raised by focus callbacks at the previous top-level event are
        // committed before resolving this event's keyboard dispatch target.
        (void)context.focusRequests.Flush(scene, context.focusManager, focusContext);
        bool performTabMove = false;
        UIFocusMoveDirection tabDirection = UIFocusMoveDirection::Forward;
        ++result.consumedInputCount;
        ++m_LastStats.inputEvents;
        if (m_DebugLoggingEnabled) {
            AppendDebug(std::visit([](const auto& value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, Engine::PointerMoveEvent>) return std::string{"[Input] PointerMove"};
                else if constexpr (std::is_same_v<T, Engine::PointerButtonEvent>) return value.pressed
                    ? std::string{"[Input] PointerDown"} : std::string{"[Input] PointerUp"};
                else if constexpr (std::is_same_v<T, Engine::PointerWheelEvent>) return std::string{"[Input] PointerWheel"};
                else if constexpr (std::is_same_v<T, Engine::PointerLeaveEvent>) return std::string{"[Input] PointerLeave"};
                else if constexpr (std::is_same_v<T, Engine::PointerCancelEvent>) return std::string{"[Input] PointerCancel"};
                else if constexpr (std::is_same_v<T, Engine::KeyEvent>) return value.pressed
                    ? std::string{"[Input] KeyDown"} : std::string{"[Input] KeyUp"};
                else if constexpr (std::is_same_v<T, Engine::TextInputEvent>) return std::string{"[Input] TextInput"};
                else return value.focused ? std::string{"[Input] WindowFocusGained"}
                    : std::string{"[Input] WindowFocusLost"};
            }, input));
        }
        std::visit([&](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, Engine::PointerMoveEvent>) {
                UIEvent event;
                event.type = UIEventType::PointerMove; event.window = value.window;
                event.pointerId = value.pointerId; event.scenePosition = value.logicalPosition;
                event.modifiers = value.modifiers; event.timestampSeconds = value.timestampSeconds;
                const UIHitTestResult physical = HitTest(scene, value.logicalPosition, context, result);
                UpdateHover(scene, value.pointerId, physical, event, context, result);
                UINodeHandle target = context.interaction.capture.GetCapturedNode(value.pointerId);
                if (!Eligible(scene, target)) {
                    if (target.IsValid() && context.interaction.capture.Release(value.pointerId)) ++m_LastStats.capturesReleased;
                    target = physical.target;
                }
                std::vector<UINodeHandle> route;
                if (BuildRoute(scene, target, route)) (void)RouteEvent(scene, event, route, context, result);
                const float threshold = std::max(0.0f, context.clickPolicy.movementThreshold);
                for (Engine::PointerButton button : kPointerButtons) {
                    UIPressRecord* press = context.interaction.press.TryGet({value.pointerId, button});
                    if (press && !press->canceled &&
                        DistanceSquared(value.logicalPosition, press->downScenePosition) > threshold * threshold) {
                        press->canceled = true;
                        MarkVisual(scene, press->pressedNode, result);
                        ++m_LastStats.pressesCanceled;
                    }
                }
            } else if constexpr (std::is_same_v<T, Engine::PointerButtonEvent>) {
                UIEvent event;
                event.type = value.pressed ? UIEventType::PointerDown : UIEventType::PointerUp;
                event.window = value.window; event.pointerId = value.pointerId;
                event.scenePosition = value.logicalPosition; event.button = value.button;
                event.modifiers = value.modifiers; event.timestampSeconds = value.timestampSeconds;
                const UIHitTestResult physical = HitTest(scene, value.logicalPosition, context, result);
                UpdateHover(scene, value.pointerId, physical, event, context, result);
                UINodeHandle target = context.interaction.capture.GetCapturedNode(value.pointerId);
                if (!Eligible(scene, target)) target = physical.target;
                std::vector<UINodeHandle> route;
                RoutedEventResult routed{event, {}};
                if (BuildRoute(scene, target, route)) routed = RouteEvent(scene, event, route, context, result);
                if (value.pressed) {
                    if (!routed.event.defaultPrevented && target.IsValid()) {
                        if (context.interaction.press.BeginPress(
                                {value.pointerId, value.button}, target,
                                value.logicalPosition, value.timestampSeconds)) {
                            MarkVisual(scene, target, result);
                            ++m_LastStats.pressesStarted;
                        }
                        if (value.button == Engine::PointerButton::Primary &&
                            !routed.requests.explicitCaptureAction &&
                            context.interaction.capture.Capture(scene, value.pointerId, target)) {
                            ++m_LastStats.capturesStarted;
                            result.interactionStateChanged = true;
                        }
                    }
                    if (!routed.event.defaultPrevented && !routed.requests.focusRequested) {
                        UINodeHandle focusRoot = context.focusManager.GetFocusRoot();
                        if (!scene.TryGet(focusRoot)) focusRoot = scene.Root();
                        bool requested = false;
                        std::vector<UINodeHandle> physicalRoute;
                        (void)BuildRoute(scene, physical.target, physicalRoute);
                        for (std::size_t i = physicalRoute.size(); i > 0; --i) {
                            const UINodeRecord* node = scene.TryGet(physicalRoute[i - 1]);
                            if (node && IsFocusable(scene, node->handle, focusRoot)) {
                                context.focusRequests.Enqueue({node->handle, UIFocusReason::Pointer,
                                    UIFocusRequestPriority::DefaultBehavior, false});
                                requested = true;
                                break;
                            }
                        }
                        if (!requested && context.focusPolicy.clearFocusOnBackgroundPointerDown) {
                            context.focusRequests.Enqueue({{}, UIFocusReason::Pointer,
                                UIFocusRequestPriority::DefaultBehavior, true});
                        }
                    }
                } else {
                    const UIPressKey key{value.pointerId, value.button};
                    const UIPressRecord* record = context.interaction.press.TryGet(key);
                    const UIPressRecord press = record ? *record : UIPressRecord{};
                    const float threshold = std::max(0.0f, context.clickPolicy.movementThreshold);
                    const bool click = record && !record->canceled && !routed.event.defaultPrevented &&
                        Eligible(scene, record->pressedNode) && physical.target == record->pressedNode &&
                        DistanceSquared(value.logicalPosition, record->downScenePosition) <= threshold * threshold;
                    if (click) {
                        std::vector<UINodeHandle> clickRoute;
                        if (BuildRoute(scene, press.pressedNode, clickRoute)) {
                            UIEvent clickEvent = event;
                            clickEvent.type = UIEventType::Click;
                            (void)RouteEvent(scene, clickEvent, clickRoute, context, result);
                            ++result.clickCount;
                            ++m_LastStats.clicksGenerated;
                        }
                    }
                    if (record) MarkVisual(scene, press.pressedNode, result);
                    (void)context.interaction.press.EndPress(key);
                    if (context.interaction.capture.Release(value.pointerId)) {
                        ++m_LastStats.capturesReleased;
                        result.interactionStateChanged = true;
                    }
                }
            } else if constexpr (std::is_same_v<T, Engine::PointerWheelEvent>) {
                const UIHitTestResult physical = HitTest(scene, value.logicalPosition, context, result);
                UINodeHandle target = physical.hit ? physical.target : scene.Root();
                std::vector<UINodeHandle> route;
                if (BuildRoute(scene, target, route)) {
                    UIEvent event;
                    event.type = UIEventType::PointerWheel; event.window = value.window;
                    event.pointerId = value.pointerId; event.scenePosition = value.logicalPosition;
                    event.wheelDelta = value.logicalDelta; event.modifiers = value.modifiers;
                    event.timestampSeconds = value.timestampSeconds;
                    (void)RouteEvent(scene, event, route, context, result);
                }
            } else if constexpr (std::is_same_v<T, Engine::PointerLeaveEvent>) {
                UIEvent source;
                source.window = value.window; source.pointerId = value.pointerId;
                source.timestampSeconds = value.timestampSeconds;
                const UIHoverTransition transition = context.interaction.hover.Clear(value.pointerId);
                DispatchHoverTransition(scene, value.pointerId, transition, source, context, result);
            } else if constexpr (std::is_same_v<T, Engine::PointerCancelEvent>) {
                UINodeHandle target = context.interaction.capture.GetCapturedNode(value.pointerId);
                std::vector<UINodeHandle> route;
                if (BuildRoute(scene, target, route)) {
                    UIEvent event;
                    event.type = UIEventType::PointerCancel; event.window = value.window;
                    event.pointerId = value.pointerId; event.scenePosition = value.logicalPosition;
                    event.timestampSeconds = value.timestampSeconds;
                    (void)RouteEvent(scene, event, route, context, result);
                }
                for (Engine::PointerButton button : kPointerButtons) {
                    if (UIPressRecord* press = context.interaction.press.TryGet({value.pointerId, button})) {
                        MarkVisual(scene, press->pressedNode, result);
                        (void)context.interaction.press.EndPress({value.pointerId, button});
                        ++m_LastStats.pressesCanceled;
                    }
                }
                if (context.interaction.capture.Release(value.pointerId)) ++m_LastStats.capturesReleased;
                result.interactionStateChanged = true;
            } else if constexpr (std::is_same_v<T, Engine::KeyEvent>) {
                if (!context.focusManager.IsWindowFocused()) return;
                std::vector<UINodeHandle> route;
                if (BuildRoute(scene, ResolveKeyboardTarget(scene, context.focusManager), route)) {
                    UIEvent event;
                    event.type = value.pressed ? UIEventType::KeyDown : UIEventType::KeyUp;
                    event.window = value.window; event.key = value.key; event.repeat = value.repeat;
                    event.modifiers = value.modifiers; event.timestampSeconds = value.timestampSeconds;
                    RoutedEventResult routed = RouteEvent(scene, event, route, context, result);
                    if (value.pressed && value.key == Engine::InputKeyCode::Tab &&
                        !routed.event.defaultPrevented && !routed.requests.focusRequested) {
                        const bool backward =
                            (static_cast<std::uint8_t>(value.modifiers) &
                             static_cast<std::uint8_t>(Engine::InputModifiers::Shift)) != 0;
                        performTabMove = true;
                        tabDirection = backward
                            ? UIFocusMoveDirection::Backward : UIFocusMoveDirection::Forward;
                    }
                }
            } else if constexpr (std::is_same_v<T, Engine::TextInputEvent>) {
                if (!context.focusManager.IsWindowFocused()) return;
                std::vector<UINodeHandle> route;
                if (BuildRoute(scene, ResolveKeyboardTarget(scene, context.focusManager), route)) {
                    UIEvent event;
                    event.type = UIEventType::TextInput; event.window = value.window;
                    event.codepoint = value.codepoint; event.timestampSeconds = value.timestampSeconds;
                    (void)RouteEvent(scene, event, route, context, result);
                }
            } else if constexpr (std::is_same_v<T, Engine::WindowFocusEvent>) {
                if (!value.focused) HandleFocusLost(scene, value, context, result);
                else {
                    UIFocusDispatchContext focusContext{
                        context.window, context.listeners, context.mutationQueue,
                        context.interaction.capture, context.focusRequests, context.focusPolicy};
                    context.focusManager.OnWindowFocusChanged(scene, true, focusContext);
                    std::vector<UINodeHandle> route;
                    if (BuildRoute(scene, ResolveKeyboardTarget(scene, context.focusManager), route)) {
                        UIEvent event;
                        event.type = UIEventType::WindowFocusGained; event.window = value.window;
                        event.timestampSeconds = value.timestampSeconds;
                        (void)RouteEvent(scene, event, route, context, result);
                    }
                }
            }
        }, input);

        (void)context.focusRequests.Flush(scene, context.focusManager, focusContext);
        if (performTabMove) {
            (void)context.focusManager.MoveFocus(
                scene, tabDirection, {context.focusPolicy.wrapTabNavigation}, focusContext);
        }
        if (scene.VisualRevision() != visualBeforeFocus) {
            result.visualInvalidated = true;
            result.interactionStateChanged = true;
        }
        context.listeners.EndDispatch();
    }
    m_ActiveContext = nullptr;
    m_DispatchInProgress = false;
    if (result.interactionStateChanged) context.interaction.Touch();
    result.mutationQueued = result.mutationQueued || !context.mutationQueue.Empty();
    return result;
}

void BasicUIInputRouter::OnNodeInvalidated(UIScene&, UINodeHandle node) {
    if (!m_ActiveContext) return;
    m_ActiveContext->listeners.RemoveAllForNode(node);
    m_ActiveContext->interaction.OnNodeInvalidated(node);
}

void BasicUIInputRouter::OnWindowHidden(UIScene&) {
    if (m_ActiveContext) m_ActiveContext->interaction.ClearAll();
}

void BasicUIInputRouter::OnWindowFocusLost(UIScene&) {
    if (m_ActiveContext) m_ActiveContext->interaction.ClearAll();
}

void BasicUIInputRouter::AppendDebug(const std::string& line) {
    if (!m_DebugLoggingEnabled) return;
    m_LastDebugLog += line;
    m_LastDebugLog.push_back('\n');
}

} // namespace Engine::UI2D
