#pragma once

#include "Foundation/Math/Matrix3.h"

namespace Engine::UI2D {

struct UIComputedTransform {
    Engine::Mat3F localToParent{};
    Engine::Mat3F localToScene{};
    Engine::Mat3F sceneToLocal{};
    bool inverseValid = true;
};

struct UILayoutState {
    Engine::Vec2F desiredSize{};
    Engine::RectF arrangedRect{};
    Engine::RectF contentRect{};
    // Final visual-space AABB after parent transforms, pivot, scale and rotation.
    Engine::RectF sceneRect{};
    UIComputedTransform computedTransform{};
    float effectiveOpacity = 1.0f;
    bool effectiveVisible = true;
    bool effectiveEnabled = true;
    bool measureValid = false;
    bool arrangeValid = false;
};

} // namespace Engine::UI2D
