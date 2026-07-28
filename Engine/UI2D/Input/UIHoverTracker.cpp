#include "UI2D/Input/UIHoverTracker.h"

#include "UI2D/Core/UIScene.h"

#include <algorithm>

namespace Engine::UI2D {

std::span<const UINodeHandle> UIHoverTracker::CurrentRoute(Engine::PointerId pointerId) const {
    const auto it = m_Routes.find(pointerId);
    return it == m_Routes.end() ? std::span<const UINodeHandle>{} : std::span<const UINodeHandle>{it->second};
}

UIHoverTransition UIHoverTracker::Update(
    Engine::PointerId pointerId, const UIHitTestResult& hit) {
    std::vector<UINodeHandle>& oldRoute = m_Routes[pointerId];
    const std::vector<UINodeHandle> empty;
    const std::vector<UINodeHandle>& newRoute = hit.hit ? hit.route : empty;
    std::size_t common = 0;
    while (common < oldRoute.size() && common < newRoute.size() && oldRoute[common] == newRoute[common]) ++common;
    UIHoverTransition transition;
    for (std::size_t i = oldRoute.size(); i > common; --i) transition.leaves.push_back(oldRoute[i - 1]);
    for (std::size_t i = common; i < newRoute.size(); ++i) transition.enters.push_back(newRoute[i]);
    oldRoute = newRoute;
    if (oldRoute.empty()) m_Routes.erase(pointerId);
    return transition;
}

UIHoverTransition UIHoverTracker::Clear(Engine::PointerId pointerId) {
    UIHoverTransition transition;
    const auto it = m_Routes.find(pointerId);
    if (it == m_Routes.end()) return transition;
    for (std::size_t i = it->second.size(); i > 0; --i) transition.leaves.push_back(it->second[i - 1]);
    m_Routes.erase(it);
    return transition;
}

std::vector<Engine::PointerId> UIHoverTracker::Pointers() const {
    std::vector<Engine::PointerId> result;
    result.reserve(m_Routes.size());
    for (const auto& [pointerId, route] : m_Routes) {
        (void)route;
        result.push_back(pointerId);
    }
    return result;
}

std::vector<UIHoverTransition> UIHoverTracker::ClearAll() {
    std::vector<UIHoverTransition> result;
    result.reserve(m_Routes.size());
    const std::vector<Engine::PointerId> ids = Pointers();
    for (Engine::PointerId id : ids) result.push_back(Clear(id));
    return result;
}

void UIHoverTracker::OnNodeInvalidated(UINodeHandle node) {
    for (auto it = m_Routes.begin(); it != m_Routes.end();) {
        auto found = std::find(it->second.begin(), it->second.end(), node);
        if (found != it->second.end()) it->second.erase(found, it->second.end());
        if (it->second.empty()) it = m_Routes.erase(it); else ++it;
    }
}

std::vector<UINodeHandle> UIHoverTracker::Sanitize(const UIScene& scene) {
    std::vector<UINodeHandle> removed;
    for (auto it = m_Routes.begin(); it != m_Routes.end();) {
        auto invalid = std::find_if(it->second.begin(), it->second.end(), [&](UINodeHandle handle) {
            const UINodeRecord* node = scene.TryGet(handle);
            return !node || !node->visible || !node->enabled ||
                !node->layoutState.effectiveVisible || !node->layoutState.effectiveEnabled;
        });
        if (invalid != it->second.end()) {
            removed.insert(removed.end(), invalid, it->second.end());
            it->second.erase(invalid, it->second.end());
        }
        if (it->second.empty()) it = m_Routes.erase(it); else ++it;
    }
    return removed;
}

bool UIHoverTracker::IsHovered(UINodeHandle node) const {
    for (const auto& [pointerId, route] : m_Routes) {
        (void)pointerId;
        if (std::find(route.begin(), route.end(), node) != route.end()) return true;
    }
    return false;
}

} // namespace Engine::UI2D
