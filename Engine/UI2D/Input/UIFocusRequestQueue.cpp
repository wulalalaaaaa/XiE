#include "UI2D/Input/UIFocusRequestQueue.h"

#include "UI2D/Core/UIScene.h"
#include "UI2D/Input/IUIFocusManager.h"

#include <algorithm>

namespace Engine::UI2D {

void UIFocusRequestQueue::Enqueue(UIFocusRequest request) {
    m_Pending.push_back(request);
}

UIFocusRequestResult UIFocusRequestQueue::Flush(
    UIScene& scene,
    IUIFocusManager& focusManager,
    UIFocusDispatchContext& context) {
    if (m_FlushInProgress || m_Pending.empty()) {
        return {UIFocusRequestStatus::Deferred,
            focusManager.GetFocusedNode(), focusManager.GetFocusedNode()};
    }

    m_FlushInProgress = true;
    const std::size_t flushCount = m_Pending.size();
    std::vector<UIFocusRequest> current(
        m_Pending.begin(), m_Pending.begin() + static_cast<std::ptrdiff_t>(flushCount));
    m_Pending.erase(m_Pending.begin(), m_Pending.begin() + static_cast<std::ptrdiff_t>(flushCount));

    UIFocusRequestResult result{UIFocusRequestStatus::Deferred,
        focusManager.GetFocusedNode(), focusManager.GetFocusedNode()};
    for (int priority = static_cast<int>(UIFocusRequestPriority::Lifecycle);
         priority >= static_cast<int>(UIFocusRequestPriority::DefaultBehavior); --priority) {
        for (std::size_t i = current.size(); i > 0; --i) {
            const UIFocusRequest& request = current[i - 1];
            if (static_cast<int>(request.priority) != priority) continue;
            result = request.clearFocus
                ? focusManager.ClearFocus(scene, request.reason, context)
                : focusManager.RequestFocus(scene, request, context);
            if (result.status == UIFocusRequestStatus::RejectedInvalidTarget ||
                result.status == UIFocusRequestStatus::RejectedNotFocusable) continue;
            m_FlushInProgress = false;
            return result;
        }
    }

    if (!current.empty()) {
        const bool lifecycle = std::any_of(current.begin(), current.end(), [](const UIFocusRequest& request) {
            return request.priority == UIFocusRequestPriority::Lifecycle;
        });
        if (lifecycle) {
            result = focusManager.ClearFocus(scene, UIFocusReason::NodeInvalidated, context);
            m_FlushInProgress = false;
            return result;
        }
        result.status = scene.TryGet(current.back().target)
            ? UIFocusRequestStatus::RejectedNotFocusable
            : UIFocusRequestStatus::RejectedInvalidTarget;
    }
    m_FlushInProgress = false;
    return result;
}

} // namespace Engine::UI2D
