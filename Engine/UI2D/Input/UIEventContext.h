#pragma once

#include "Input/InputEvents.h"
#include "UI2D/Core/UISceneMutationQueue.h"
#include "UI2D/Input/UIEvent.h"

namespace Engine::UI2D {

class UIFocusRequestQueue;
class UIPointerCaptureService;
class UIPressTracker;
class UIScene;

struct UIEventDispatchRequests {
    bool explicitCaptureAction = false;
    bool focusRequested = false;
    bool mutationQueued = false;
    bool interactionStateChanged = false;
};

class UIEventContext {
public:
    UIEventContext(
        UIEvent& event,
        UIScene& scene,
        UIFocusRequestQueue& focusRequests,
        UIPointerCaptureService& captureService,
        UISceneMutationQueue& mutationQueue,
        UIEventDispatchRequests& requests,
        UIPressTracker* pressTracker = nullptr);

    [[nodiscard]] UIEvent& Event() noexcept { return m_Event; }
    void MarkHandled() noexcept { m_Event.handled = true; }
    void PreventDefault() noexcept { m_Event.defaultPrevented = true; }
    void StopPropagation() noexcept { m_Event.propagationStopped = true; }
    void StopImmediatePropagation() noexcept {
        m_Event.immediatePropagationStopped = true;
        m_Event.propagationStopped = true;
    }

    bool RequestPointerCapture(Engine::PointerId pointerId, UINodeHandle node);
    void ReleasePointerCapture(Engine::PointerId pointerId);
    bool CancelClick(Engine::PointerId pointerId, Engine::PointerButton button);
    bool RequestFocus(UINodeHandle node);
    void ClearFocus(UIFocusReason reason = UIFocusReason::Clear);
    void EnqueueMutation(UISceneMutation mutation);

private:
    UIEvent& m_Event;
    UIScene& m_Scene;
    UIFocusRequestQueue& m_FocusRequests;
    UIPointerCaptureService& m_CaptureService;
    UISceneMutationQueue& m_MutationQueue;
    UIEventDispatchRequests& m_Requests;
    UIPressTracker* m_PressTracker = nullptr;
};

} // namespace Engine::UI2D
