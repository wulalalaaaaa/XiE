#pragma once

#include "Input/InputEvents.h"
#include "UI2D/Core/UINodeHandle.h"
#include "UI2D/HitTest/UIHitTestTypes.h"

#include <span>
#include <unordered_map>
#include <vector>

namespace Engine::UI2D {

class UIScene;

struct UIHoverTransition {
    std::vector<UINodeHandle> leaves;
    std::vector<UINodeHandle> enters;
    [[nodiscard]] bool Changed() const noexcept { return !leaves.empty() || !enters.empty(); }
};

class UIHoverTracker {
public:
    [[nodiscard]] std::span<const UINodeHandle> CurrentRoute(Engine::PointerId pointerId) const;
    UIHoverTransition Update(Engine::PointerId pointerId, const UIHitTestResult& hit);
    UIHoverTransition Clear(Engine::PointerId pointerId);
    [[nodiscard]] std::vector<Engine::PointerId> Pointers() const;
    std::vector<UIHoverTransition> ClearAll();
    void OnNodeInvalidated(UINodeHandle node);
    std::vector<UINodeHandle> Sanitize(const UIScene& scene);
    [[nodiscard]] bool IsHovered(UINodeHandle node) const;

private:
    std::unordered_map<Engine::PointerId, std::vector<UINodeHandle>> m_Routes;
};

} // namespace Engine::UI2D
