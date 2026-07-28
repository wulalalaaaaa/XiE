#pragma once

#include "Foundation/Math/Types2D.h"
#include "UI2D/HitTest/UIHitShape.h"

namespace Engine::UI2D {

bool PointInRect(Engine::Vec2F point, const Engine::RectF& rect) noexcept;
bool PointInRoundedRect(Engine::Vec2F point, const Engine::RectF& rect, float radius) noexcept;
bool PointInCircle(Engine::Vec2F point, Engine::Vec2F center, float radius) noexcept;
bool PointInRing(
    Engine::Vec2F point, Engine::Vec2F center, float innerRadius, float outerRadius) noexcept;

// Resolves defaults and normalizes radii before applying a shape. Invalid or
// non-positive rectangles safely return false.
bool PointInHitShape(
    Engine::Vec2F point, const Engine::RectF& localRect, const UIHitShape& shape) noexcept;

} // namespace Engine::UI2D
