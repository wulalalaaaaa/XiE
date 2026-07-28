#include "UI2D/Input/BasicUIFocusManager.h"

#include "UI2D/Core/UIScene.h"
#include "Foundation/Math/Matrix3.h"
#include "UI2D/Input/UIEventContext.h"
#include "UI2D/Input/UIEventListenerRegistry.h"
#include "UI2D/Input/UIFocusRequestQueue.h"
#include "UI2D/Input/UIFocusUtils.h"

#include <algorithm>
#include <sstream>

namespace Engine::UI2D {

bool BasicUIFocusManager::HasFocusWithin(UINodeHandle node) const {
    return HasActiveFocus() &&
        std::find(m_State.focusRoute.begin(), m_State.focusRoute.end(), node) != m_State.focusRoute.end();
}

void BasicUIFocusManager::EnsureFocusRoot(UIScene& scene) {
    if (!scene.TryGet(m_State.focusRoot)) m_State.focusRoot = scene.Root();
}

bool BasicUIFocusManager::BuildRoute(
    const UIScene& scene, UINodeHandle target, std::vector<UINodeHandle>& route) const {
    route.clear();
    for (UINodeHandle current = target; current.IsValid();) {
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

void BasicUIFocusManager::DispatchFocusEvent(
    UIScene& scene,
    UIEventType type,
    UINodeHandle target,
    UINodeHandle related,
    UIFocusReason reason,
    std::span<const UINodeHandle> route,
    UIFocusDispatchContext& context,
    bool* prevented) {
    if (route.empty() || !scene.TryGet(target)) return;
    UIEvent event;
    event.type = type;
    event.window = context.window;
    event.target = target;
    event.relatedTarget = related;
    event.focusReason = reason;
    UIEventDispatchRequests requests;

    auto invoke = [&](UINodeHandle current, UIEventPhase phase) {
        const UINodeRecord* record = scene.TryGet(current);
        if (!record) return;
        event.phase = phase;
        event.currentTarget = current;
        if (record->layoutState.computedTransform.inverseValid) {
            event.localPosition = Engine::TransformPoint(
                record->layoutState.computedTransform.sceneToLocal, event.scenePosition);
        }
        UIEventContext eventContext(
            event, scene, context.requestQueue, context.captureService,
            context.mutationQueue, requests);
        (void)context.listeners.Invoke(current, type, phase, eventContext);
    };

    for (UINodeHandle node : route) {
        invoke(node, UIEventPhase::Preview);
        if (event.propagationStopped) break;
    }
    if (!event.propagationStopped) invoke(target, UIEventPhase::Target);
    if (!event.propagationStopped) {
        for (std::size_t i = route.size(); i > 0; --i) {
            invoke(route[i - 1], UIEventPhase::Bubble);
            if (event.propagationStopped) break;
        }
    }
    if (prevented) *prevented = event.defaultPrevented;
}

void BasicUIFocusManager::MarkRouteChange(
    UIScene& scene,
    std::span<const UINodeHandle> oldRoute,
    std::span<const UINodeHandle> newRoute) {
    scene.InvalidateFocusRevision();
    for (UINodeHandle node : oldRoute) {
        if (std::find(newRoute.begin(), newRoute.end(), node) == newRoute.end())
            { (void)scene.MarkDirty(node, UIDirtyFlags::Visual); scene.MarkStyleDirty(node); }
    }
    for (UINodeHandle node : newRoute) {
        if (std::find(oldRoute.begin(), oldRoute.end(), node) == oldRoute.end())
            { (void)scene.MarkDirty(node, UIDirtyFlags::Visual); scene.MarkStyleDirty(node); }
    }
    if (!oldRoute.empty()) { (void)scene.MarkDirty(oldRoute.back(), UIDirtyFlags::Visual); scene.MarkStyleDirty(oldRoute.back()); }
    if (!newRoute.empty()) { (void)scene.MarkDirty(newRoute.back(), UIDirtyFlags::Visual); scene.MarkStyleDirty(newRoute.back()); }
}

UIFocusRequestResult BasicUIFocusManager::ApplyFocus(
    UIScene& scene,
    UINodeHandle target,
    UIFocusReason reason,
    UIFocusRequestPriority priority,
    UIFocusDispatchContext& context) {
    EnsureFocusRoot(scene);
    ++m_Stats.requests;
    const UINodeHandle old = m_State.focusedNode;
    AppendDebug("[Focus] request node=" + std::to_string(target.index) +
        " reason=" + std::to_string(static_cast<int>(reason)));
    if (!scene.TryGet(target)) {
        ++m_Stats.rejected;
        return {UIFocusRequestStatus::RejectedInvalidTarget, old, old};
    }
    if (!IsFocusable(scene, target, m_State.focusRoot)) {
        ++m_Stats.rejected;
        return {UIFocusRequestStatus::RejectedNotFocusable, old, old};
    }
    if (target == old && m_State.windowFocused) {
        return {UIFocusRequestStatus::AlreadyFocused, old, old};
    }
    if (!m_State.windowFocused) {
        m_State.restoreCandidate = target;
        return {UIFocusRequestStatus::Deferred, old, old};
    }

    std::vector<UINodeHandle> newRoute;
    if (!BuildRoute(scene, target, newRoute)) {
        ++m_Stats.rejected;
        return {UIFocusRequestStatus::RejectedInvalidTarget, old, old};
    }
    const bool ownDispatch = !context.listeners.DispatchInProgress();
    if (ownDispatch) context.listeners.BeginDispatch();
    bool prevented = false;
    DispatchFocusEvent(scene, UIEventType::FocusChanging, target, old, reason, newRoute, context, &prevented);
    const bool preventable = priority != UIFocusRequestPriority::Lifecycle && !IsLifecycleFocusReason(reason);
    if (prevented && preventable) {
        if (ownDispatch) context.listeners.EndDispatch();
        ++m_Stats.prevented;
        AppendDebug("[Focus] prevented");
        return {UIFocusRequestStatus::RejectedPrevented, old, old};
    }

    const std::vector<UINodeHandle> oldRoute = m_State.focusRoute;
    m_State.focusedNode = target;
    m_State.focusRoute = newRoute;
    if (old.IsValid() && scene.TryGet(old)) {
        DispatchFocusEvent(scene, UIEventType::FocusLost, old, target, reason, oldRoute, context);
        ++m_Stats.focusLostEvents;
    }
    DispatchFocusEvent(scene, UIEventType::FocusGained, target, old, reason, newRoute, context);
    ++m_Stats.focusGainedEvents;
    MarkRouteChange(scene, oldRoute, newRoute);
    if (ownDispatch) context.listeners.EndDispatch();
    ++m_Stats.applied;
    if (reason == UIFocusReason::Restore) ++m_Stats.restores;
    AppendDebug("[Focus] applied");
    return {UIFocusRequestStatus::Applied, old, target};
}

UIFocusRequestResult BasicUIFocusManager::RequestFocus(
    UIScene& scene, const UIFocusRequest& request, UIFocusDispatchContext& context) {
    if (request.clearFocus) return ClearFocus(scene, request.reason, context);
    return ApplyFocus(scene, request.target, request.reason, request.priority, context);
}

UIFocusRequestResult BasicUIFocusManager::ClearFocus(
    UIScene& scene, UIFocusReason reason, UIFocusDispatchContext& context) {
    const UINodeHandle old = m_State.focusedNode;
    if (!old.IsValid()) return {UIFocusRequestStatus::AlreadyFocused, {}, {}};
    ++m_Stats.requests;
    const std::vector<UINodeHandle> oldRoute = m_State.focusRoute;
    const bool lifecycle = IsLifecycleFocusReason(reason) || reason == UIFocusReason::WindowDeactivated;
    const bool ownDispatch = !context.listeners.DispatchInProgress();
    if (ownDispatch) context.listeners.BeginDispatch();
    bool prevented = false;
    if (scene.TryGet(old)) {
        DispatchFocusEvent(scene, UIEventType::FocusChanging, old, {}, reason, oldRoute, context, &prevented);
    }
    if (prevented && !lifecycle) {
        if (ownDispatch) context.listeners.EndDispatch();
        ++m_Stats.prevented;
        return {UIFocusRequestStatus::RejectedPrevented, old, old};
    }
    m_State.focusedNode = {};
    m_State.focusRoute.clear();
    if (scene.TryGet(old)) {
        DispatchFocusEvent(scene, UIEventType::FocusLost, old, {}, reason, oldRoute, context);
        ++m_Stats.focusLostEvents;
    }
    MarkRouteChange(scene, oldRoute, {});
    if (ownDispatch) context.listeners.EndDispatch();
    ++m_Stats.applied;
    return {UIFocusRequestStatus::Cleared, old, {}};
}

void BasicUIFocusManager::AppendCandidates(UIScene& scene, UINodeHandle node) {
    const UINodeRecord* record = scene.TryGet(node);
    if (!record) return;
    if (record->focus.tabIndex >= 0 && IsFocusable(scene, node, m_State.focusRoot))
        m_FocusCandidates.push_back(node);
    std::vector<UINodeHandle> children(scene.Children(node).begin(), scene.Children(node).end());
    std::stable_sort(children.begin(), children.end(), [&](UINodeHandle lhs, UINodeHandle rhs) {
        return scene.TryGet(lhs)->insertionOrder < scene.TryGet(rhs)->insertionOrder;
    });
    for (UINodeHandle child : children) AppendCandidates(scene, child);
}

void BasicUIFocusManager::RebuildCandidates(UIScene& scene) {
    EnsureFocusRoot(scene);
    m_FocusCandidates.clear();
    AppendCandidates(scene, m_State.focusRoot);
    std::stable_sort(m_FocusCandidates.begin(), m_FocusCandidates.end(), [&](UINodeHandle lhs, UINodeHandle rhs) {
        return scene.TryGet(lhs)->focus.tabIndex < scene.TryGet(rhs)->focus.tabIndex;
    });
    m_CachedFocusRevision = scene.FocusRevision();
    m_CachedFocusRoot = m_State.focusRoot;
    ++m_Stats.candidateRebuilds;
}

std::span<const UINodeHandle> BasicUIFocusManager::FocusCandidates(UIScene& scene) {
    EnsureFocusRoot(scene);
    if (m_CachedFocusRevision != scene.FocusRevision() || m_CachedFocusRoot != m_State.focusRoot)
        RebuildCandidates(scene);
    return m_FocusCandidates;
}

UIFocusMoveResult BasicUIFocusManager::MoveFocus(
    UIScene& scene,
    UIFocusMoveDirection direction,
    const UIFocusMoveContext& moveContext,
    UIFocusDispatchContext& dispatchContext) {
    const std::span<const UINodeHandle> candidates = FocusCandidates(scene);
    UIFocusMoveResult result;
    result.direction = direction;
    if (candidates.empty()) return result;
    auto current = std::find(candidates.begin(), candidates.end(), m_State.focusedNode);
    std::size_t index = 0;
    if (current == candidates.end()) {
        index = direction == UIFocusMoveDirection::Forward ? 0 : candidates.size() - 1;
    } else if (direction == UIFocusMoveDirection::Forward) {
        const std::size_t currentIndex = static_cast<std::size_t>(current - candidates.begin());
        if (currentIndex + 1 < candidates.size()) index = currentIndex + 1;
        else if (moveContext.wrap) index = 0;
        else { result.request = {UIFocusRequestStatus::AlreadyFocused, *current, *current}; return result; }
    } else {
        const std::size_t currentIndex = static_cast<std::size_t>(current - candidates.begin());
        if (currentIndex > 0) index = currentIndex - 1;
        else if (moveContext.wrap) index = candidates.size() - 1;
        else { result.request = {UIFocusRequestStatus::AlreadyFocused, *current, *current}; return result; }
    }
    result.request = ApplyFocus(scene, candidates[index], UIFocusReason::KeyboardTab,
        UIFocusRequestPriority::DefaultBehavior, dispatchContext);
    result.moved = result.request.status == UIFocusRequestStatus::Applied;
    if (result.moved) ++m_Stats.tabMoves;
    if (result.moved) AppendDebug("[Focus] tab moved");
    return result;
}

void BasicUIFocusManager::SetFocusRoot(
    UIScene& scene, UINodeHandle root, UIFocusDispatchContext& context) {
    if (!scene.TryGet(root)) return;
    EnsureFocusRoot(scene);
    if (m_State.focusRoot == root) return;
    const std::vector<UINodeHandle> oldRoute = m_State.focusRoute;
    m_State.focusRoot = root;
    scene.InvalidateFocusRevision();
    m_CachedFocusRevision = 0;
    if (IsFocusable(scene, m_State.focusedNode, root)) {
        std::vector<UINodeHandle> route;
        (void)BuildRoute(scene, m_State.focusedNode, route);
        m_State.focusRoute = route;
        MarkRouteChange(scene, oldRoute, route);
        return;
    }
    const std::span<const UINodeHandle> candidates = FocusCandidates(scene);
    if (!candidates.empty()) {
        (void)ApplyFocus(scene, candidates.front(), UIFocusReason::FocusRootChanged,
            UIFocusRequestPriority::Lifecycle, context);
    } else {
        (void)ClearFocus(scene, UIFocusReason::FocusRootChanged, context);
    }
}

void BasicUIFocusManager::OnWindowFocusChanged(
    UIScene& scene, bool focused, UIFocusDispatchContext& context) {
    EnsureFocusRoot(scene);
    if (m_State.windowFocused == focused) return;
    scene.MarkAllStyleDirty();
    if (!focused) {
        AppendDebug("[Focus] window deactivated");
        m_State.restoreCandidate = m_State.focusedNode;
        const UINodeHandle old = m_State.focusedNode;
        const std::vector<UINodeHandle> oldRoute = m_State.focusRoute;
        const bool ownDispatch = !context.listeners.DispatchInProgress();
        if (ownDispatch) context.listeners.BeginDispatch();
        if (scene.TryGet(old)) {
            DispatchFocusEvent(scene, UIEventType::FocusLost, old, {},
                UIFocusReason::WindowDeactivated, oldRoute, context);
            ++m_Stats.focusLostEvents;
        }
        m_State.focusedNode = {};
        m_State.focusRoute.clear();
        m_State.windowFocused = false;
        MarkRouteChange(scene, oldRoute, {});
        if (ownDispatch) context.listeners.EndDispatch();
        return;
    }

    m_State.windowFocused = true;
    if (context.policy.restoreFocusOnWindowActivated &&
        IsFocusable(scene, m_State.restoreCandidate, m_State.focusRoot)) {
        context.requestQueue.Enqueue({m_State.restoreCandidate, UIFocusReason::Restore,
            UIFocusRequestPriority::Lifecycle, false});
        AppendDebug("[Focus] restore queued");
    } else {
        m_State.restoreCandidate = {};
    }
}

void BasicUIFocusManager::OnNodeInvalidated(
    UIScene& scene,
    const UIFocusInvalidation& invalidation,
    UIFocusDispatchContext& context) {
    EnsureFocusRoot(scene);
    if (invalidation.node == m_State.restoreCandidate &&
        invalidation.reason != UIFocusInvalidationReason::Reparented) {
        m_State.restoreCandidate = {};
    }
    if (invalidation.node == m_State.focusRoot && invalidation.node != scene.Root()) {
        m_State.focusRoot = scene.Root();
        m_CachedFocusRevision = 0;
    }
    if (m_State.focusedNode != invalidation.node) return;
    if (invalidation.reason == UIFocusInvalidationReason::Reparented &&
        IsFocusable(scene, m_State.focusedNode, m_State.focusRoot)) {
        std::vector<UINodeHandle> nextRoute;
        if (BuildRoute(scene, m_State.focusedNode, nextRoute)) {
            const std::vector<UINodeHandle> oldRoute = m_State.focusRoute;
            m_State.focusRoute = nextRoute;
            MarkRouteChange(scene, oldRoute, nextRoute);
        }
        return;
    }

    UINodeHandle ancestor = invalidation.lastKnownParent;
    if (!ancestor.IsValid()) {
        if (const UINodeRecord* node = scene.TryGet(invalidation.node)) ancestor = node->parent;
    }
    while (ancestor.IsValid()) {
        if (IsFocusable(scene, ancestor, m_State.focusRoot)) {
            context.requestQueue.Enqueue({ancestor, UIFocusReason::NodeInvalidated,
                UIFocusRequestPriority::Lifecycle, false});
            ++m_Stats.invalidationFallbacks;
            return;
        }
        const UINodeRecord* record = scene.TryGet(ancestor);
        ancestor = record ? record->parent : UINodeHandle{};
    }

    std::vector<UINodeHandle> previousCandidates = m_FocusCandidates;
    if (std::find(previousCandidates.begin(), previousCandidates.end(), invalidation.node) ==
        previousCandidates.end()) {
        std::vector<UINodeHandle> traversal;
        const auto append = [&](const auto& self, UINodeHandle node) -> void {
            const UINodeRecord* record = scene.TryGet(node);
            if (!record) return;
            if (record->focus.focusable && record->focus.tabIndex >= 0) traversal.push_back(node);
            std::vector<UINodeHandle> children(scene.Children(node).begin(), scene.Children(node).end());
            std::stable_sort(children.begin(), children.end(), [&](UINodeHandle lhs, UINodeHandle rhs) {
                return scene.TryGet(lhs)->insertionOrder < scene.TryGet(rhs)->insertionOrder;
            });
            for (UINodeHandle child : children) self(self, child);
        };
        append(append, m_State.focusRoot);
        std::stable_sort(traversal.begin(), traversal.end(), [&](UINodeHandle lhs, UINodeHandle rhs) {
            return scene.TryGet(lhs)->focus.tabIndex < scene.TryGet(rhs)->focus.tabIndex;
        });
        previousCandidates = std::move(traversal);
    }
    auto old = std::find(previousCandidates.begin(), previousCandidates.end(), invalidation.node);
    if (old != previousCandidates.end()) {
        UINodeHandle previous{};
        UINodeHandle next{};
        for (auto it = old + 1; it != previousCandidates.end(); ++it) {
            if (*it != invalidation.node && IsFocusable(scene, *it, m_State.focusRoot)) {
                next = *it;
                break;
            }
        }
        for (auto it = old; it != previousCandidates.begin();) {
            --it;
            if (*it != invalidation.node && IsFocusable(scene, *it, m_State.focusRoot)) {
                previous = *it;
                break;
            }
        }
        // Queue processes the last valid request first: previous is the fallback
        // if the preferred next candidate disappears before the lifecycle flush.
        if (previous.IsValid()) context.requestQueue.Enqueue({previous, UIFocusReason::NodeInvalidated,
            UIFocusRequestPriority::Lifecycle, false});
        if (next.IsValid()) context.requestQueue.Enqueue({next, UIFocusReason::NodeInvalidated,
            UIFocusRequestPriority::Lifecycle, false});
        if (previous.IsValid() || next.IsValid()) {
            ++m_Stats.invalidationFallbacks;
            return;
        }
    }
    context.requestQueue.Enqueue({{}, UIFocusReason::NodeInvalidated,
        UIFocusRequestPriority::Lifecycle, true});
}

void BasicUIFocusManager::AppendDebug(std::string line) {
    if (!m_DebugLoggingEnabled) return;
    m_DebugLog += std::move(line);
    m_DebugLog.push_back('\n');
}

} // namespace Engine::UI2D
