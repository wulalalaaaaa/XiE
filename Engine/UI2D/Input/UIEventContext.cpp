#include "UI2D/Input/UIEventContext.h"

#include "UI2D/Input/UIFocusRequestQueue.h"
#include "UI2D/Input/UIPointerCaptureService.h"
#include "UI2D/Input/UIPressTracker.h"
#include "UI2D/Core/UIScene.h"

namespace Engine::UI2D {

UIEventContext::UIEventContext(
    UIEvent& event,
    UIScene& scene,
    UIFocusRequestQueue& focusRequests,
    UIPointerCaptureService& captureService,
    UISceneMutationQueue& mutationQueue,
    UIEventDispatchRequests& requests,
    UIPressTracker* pressTracker)
    : m_Event(event),
      m_Scene(scene),
      m_FocusRequests(focusRequests),
      m_CaptureService(captureService),
      m_MutationQueue(mutationQueue),
      m_Requests(requests),
      m_PressTracker(pressTracker) {}

bool UIEventContext::RequestPointerCapture(Engine::PointerId pointerId, UINodeHandle node) {
    m_Requests.explicitCaptureAction = true;
    return m_CaptureService.Capture(m_Scene, pointerId, node);
}

void UIEventContext::ReleasePointerCapture(Engine::PointerId pointerId) {
    m_Requests.explicitCaptureAction = true;
    (void)m_CaptureService.Release(pointerId);
}

bool UIEventContext::CancelClick(Engine::PointerId pointerId, Engine::PointerButton button) {
    if (!m_PressTracker) return false;
    UIPressRecord* record = m_PressTracker->TryGet({pointerId, button});
    if (!record || !m_PressTracker->Cancel({pointerId, button})) return false;
    m_Scene.MarkStyleDirty(record->pressedNode);
    m_Scene.MarkDirty(record->pressedNode, UIDirtyFlags::Visual | UIDirtyFlags::HitTest);
    m_Requests.interactionStateChanged = true;
    return true;
}

bool UIEventContext::RequestFocus(UINodeHandle node) {
    m_Requests.focusRequested = true;
    m_FocusRequests.Enqueue({node, UIFocusReason::Programmatic,
        UIFocusRequestPriority::Explicit, false});
    return node.IsValid();
}

void UIEventContext::ClearFocus(UIFocusReason reason) {
    m_Requests.focusRequested = true;
    m_FocusRequests.Enqueue({{}, reason, UIFocusRequestPriority::Explicit, true});
}

void UIEventContext::EnqueueMutation(UISceneMutation mutation) {
    m_MutationQueue.Enqueue(std::move(mutation));
    m_Requests.mutationQueued = true;
}

} // namespace Engine::UI2D
