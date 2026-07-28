#include "UI2D/Layout/UIContentExtent.h"

#include "UI2D/Core/UIScene.h"

#include <algorithm>
#include <cmath>

namespace Engine::UI2D {
namespace {
float Finite(float value) noexcept { return std::isfinite(value) ? value : 0.0f; }
}

UIContentExtent ComputeUIContentExtent(const UIScene& scene, UINodeHandle parent) {
    UIContentExtent result;
    const UINodeRecord* parentNode = scene.TryGet(parent);
    if (!parentNode) return result;
    bool found = false;
    for (UINodeHandle childHandle : scene.Children(parent)) {
        const UINodeRecord* child = scene.TryGet(childHandle);
        if (!child || !child->visible || !child->layoutState.arrangeValid) continue;
        const Engine::RectF rect = child->layoutState.arrangedRect;
        const Engine::InsetsF margin = child->layout.margin;
        const Engine::Vec2F minimum{Finite(rect.x - margin.left), Finite(rect.y - margin.top)};
        const Engine::Vec2F maximum{Finite(rect.x + rect.width + margin.right),
            Finite(rect.y + rect.height + margin.bottom)};
        if (!found) { result.minimum = minimum; result.maximum = maximum; found = true; }
        else {
            result.minimum.x = std::min(result.minimum.x, minimum.x);
            result.minimum.y = std::min(result.minimum.y, minimum.y);
            result.maximum.x = std::max(result.maximum.x, maximum.x);
            result.maximum.y = std::max(result.maximum.y, maximum.y);
        }
    }
    if (!found) return result;
    result.size = {std::max(0.0f, result.maximum.x - std::min(0.0f, result.minimum.x)),
        std::max(0.0f, result.maximum.y - std::min(0.0f, result.minimum.y))};
    return result;
}

} // namespace Engine::UI2D
