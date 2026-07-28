#pragma once

namespace Engine::UI2D {

struct UIFrameActivity {
    bool hasPendingInput = false;
    bool hasPendingMutation = false;
    bool needsLayout = false;
    bool needsBuildDrawList = false;
    bool needsRender = false;
    bool hasActiveInteraction = false;
    bool hasActiveAnimation = false;

    [[nodiscard]] bool HasWork() const noexcept {
        return hasPendingInput || hasPendingMutation || needsLayout || needsBuildDrawList || needsRender ||
            hasActiveInteraction || hasActiveAnimation;
    }
};

} // namespace Engine::UI2D
