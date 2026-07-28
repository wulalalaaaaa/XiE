#pragma once

#include "Input/InputEvents.h"
#include "UI2D/Core/UINodeHandle.h"

#include <unordered_map>
#include <vector>

namespace Engine::UI2D {

class UIScene;

class UIPointerCaptureService {
public:
    bool Capture(const UIScene& scene, Engine::PointerId pointerId, UINodeHandle node);
    bool Release(Engine::PointerId pointerId);
    [[nodiscard]] UINodeHandle GetCapturedNode(Engine::PointerId pointerId) const;
    [[nodiscard]] std::vector<Engine::PointerId> CapturedPointers() const;
    [[nodiscard]] bool HasCapture() const noexcept { return !m_Captures.empty(); }
    [[nodiscard]] bool HasPointerCapture(UINodeHandle node) const;
    void OnNodeInvalidated(UINodeHandle node);
    std::vector<UINodeHandle> Sanitize(const UIScene& scene);
    void ClearAll();

private:
    std::unordered_map<Engine::PointerId, UINodeHandle> m_Captures;
};

} // namespace Engine::UI2D
