#pragma once

#include "Foundation/Math/Types2D.h"

#include <algorithm>

namespace Crystal {

struct WorldViewportBounds {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

[[nodiscard]] constexpr bool WorldRectIntersectsViewport(
    Engine::Vec2F position, Engine::Vec2F size, WorldViewportBounds viewport) noexcept {
    const float left = std::min(position.x, position.x + size.x);
    const float right = std::max(position.x, position.x + size.x);
    const float top = std::min(position.y, position.y + size.y);
    const float bottom = std::max(position.y, position.y + size.y);
    return right >= viewport.left && left <= viewport.right &&
        bottom >= viewport.top && top <= viewport.bottom;
}

[[nodiscard]] constexpr bool WorldSegmentBoundsIntersectViewport(
    Engine::Vec2F a, Engine::Vec2F b, WorldViewportBounds viewport) noexcept {
    return std::max(a.x, b.x) >= viewport.left && std::min(a.x, b.x) <= viewport.right &&
        std::max(a.y, b.y) >= viewport.top && std::min(a.y, b.y) <= viewport.bottom;
}

[[nodiscard]] constexpr bool WorldPointInRect(
    Engine::Vec2F point, Engine::Vec2F position, Engine::Vec2F size) noexcept {
    const float left = std::min(position.x, position.x + size.x);
    const float right = std::max(position.x, position.x + size.x);
    const float top = std::min(position.y, position.y + size.y);
    const float bottom = std::max(position.y, position.y + size.y);
    return point.x >= left && point.x <= right && point.y >= top && point.y <= bottom;
}

} // namespace Crystal
