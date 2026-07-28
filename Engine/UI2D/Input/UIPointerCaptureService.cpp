#include "UI2D/Input/UIPointerCaptureService.h"

#include "UI2D/Core/UIScene.h"

namespace Engine::UI2D {
namespace {
bool Eligible(const UIScene& scene, UINodeHandle handle) {
    const UINodeRecord* node = scene.TryGet(handle);
    return node && node->visible && node->enabled && node->layoutState.effectiveVisible &&
        node->layoutState.effectiveEnabled;
}
}

bool UIPointerCaptureService::Capture(
    const UIScene& scene, Engine::PointerId pointerId, UINodeHandle node) {
    if (!Eligible(scene, node)) return false;
    m_Captures[pointerId] = node;
    return true;
}

bool UIPointerCaptureService::Release(Engine::PointerId pointerId) {
    return m_Captures.erase(pointerId) != 0;
}

UINodeHandle UIPointerCaptureService::GetCapturedNode(Engine::PointerId pointerId) const {
    const auto it = m_Captures.find(pointerId);
    return it == m_Captures.end() ? UINodeHandle{} : it->second;
}

std::vector<Engine::PointerId> UIPointerCaptureService::CapturedPointers() const {
    std::vector<Engine::PointerId> result;
    result.reserve(m_Captures.size());
    for (const auto& [pointerId, node] : m_Captures) {
        (void)node;
        result.push_back(pointerId);
    }
    return result;
}

bool UIPointerCaptureService::HasPointerCapture(UINodeHandle node) const {
    for (const auto& [pointerId, captured] : m_Captures) {
        (void)pointerId;
        if (captured == node) return true;
    }
    return false;
}

void UIPointerCaptureService::OnNodeInvalidated(UINodeHandle node) {
    for (auto it = m_Captures.begin(); it != m_Captures.end();) {
        if (it->second == node) it = m_Captures.erase(it);
        else ++it;
    }
}

std::vector<UINodeHandle> UIPointerCaptureService::Sanitize(const UIScene& scene) {
    std::vector<UINodeHandle> released;
    for (auto it = m_Captures.begin(); it != m_Captures.end();) {
        if (!Eligible(scene, it->second)) {
            released.push_back(it->second);
            it = m_Captures.erase(it);
        } else ++it;
    }
    return released;
}

void UIPointerCaptureService::ClearAll() { m_Captures.clear(); }

} // namespace Engine::UI2D
