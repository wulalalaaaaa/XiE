#include "UI2D/HitTest/UIHitGeometry.h"

#include <algorithm>
#include <cmath>

namespace Engine::UI2D {
namespace {

bool Finite(Engine::Vec2F value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

bool ValidRect(const Engine::RectF& rect) noexcept {
    return std::isfinite(rect.x) && std::isfinite(rect.y) &&
        std::isfinite(rect.width) && std::isfinite(rect.height) &&
        rect.width > 0.0f && rect.height > 0.0f;
}

float DefaultRadius(const Engine::RectF& rect) noexcept {
    return std::min(rect.width, rect.height) * 0.5f;
}

} // namespace

bool PointInRect(Engine::Vec2F point, const Engine::RectF& rect) noexcept {
    if (!Finite(point) || !ValidRect(rect)) return false;
    return point.x >= rect.x && point.y >= rect.y &&
        point.x <= rect.x + rect.width && point.y <= rect.y + rect.height;
}

bool PointInRoundedRect(Engine::Vec2F point, const Engine::RectF& rect, float radius) noexcept {
    if (!PointInRect(point, rect)) return false;
    if (!std::isfinite(radius)) return false;
    const float safeRadius = std::clamp(radius, 0.0f, DefaultRadius(rect));
    if (safeRadius == 0.0f) return true;

    const float nearestX = std::clamp(point.x, rect.x + safeRadius, rect.x + rect.width - safeRadius);
    const float nearestY = std::clamp(point.y, rect.y + safeRadius, rect.y + rect.height - safeRadius);
    const float dx = point.x - nearestX;
    const float dy = point.y - nearestY;
    return dx * dx + dy * dy <= safeRadius * safeRadius;
}

bool PointInCircle(Engine::Vec2F point, Engine::Vec2F center, float radius) noexcept {
    if (!Finite(point) || !Finite(center) || !std::isfinite(radius) || radius < 0.0f) return false;
    const float dx = point.x - center.x;
    const float dy = point.y - center.y;
    return dx * dx + dy * dy <= radius * radius;
}

bool PointInRing(
    Engine::Vec2F point, Engine::Vec2F center, float innerRadius, float outerRadius) noexcept {
    if (!Finite(point) || !Finite(center) || !std::isfinite(innerRadius) ||
        !std::isfinite(outerRadius) || innerRadius < 0.0f || outerRadius < 0.0f) return false;
    const float safeInner = std::min(innerRadius, outerRadius);
    const float safeOuter = std::max(innerRadius, outerRadius);
    const float dx = point.x - center.x;
    const float dy = point.y - center.y;
    const float distanceSquared = dx * dx + dy * dy;
    return distanceSquared >= safeInner * safeInner && distanceSquared <= safeOuter * safeOuter;
}

bool PointInHitShape(
    Engine::Vec2F point, const Engine::RectF& localRect, const UIHitShape& shape) noexcept {
    if (!ValidRect(localRect)) return false;
    switch (shape.type) {
    case UIHitShapeType::None:
        return false;
    case UIHitShapeType::Rect:
        return PointInRect(point, localRect);
    case UIHitShapeType::RoundedRect:
        return PointInRoundedRect(point, localRect, shape.cornerRadius);
    case UIHitShapeType::Circle: {
        if (!std::isfinite(shape.outerRadius)) return false;
        const float defaultRadius = DefaultRadius(localRect);
        const float radius = shape.outerRadius > 0.0f
            ? std::min(shape.outerRadius, defaultRadius) : defaultRadius;
        return PointInCircle(point,
            {localRect.x + localRect.width * 0.5f, localRect.y + localRect.height * 0.5f}, radius);
    }
    case UIHitShapeType::Ring: {
        if (!std::isfinite(shape.innerRadius) || !std::isfinite(shape.outerRadius)) return false;
        const float defaultOuter = DefaultRadius(localRect);
        const bool unspecified = shape.innerRadius <= 0.0f && shape.outerRadius <= 0.0f;
        const float outer = shape.outerRadius > 0.0f
            ? std::min(shape.outerRadius, defaultOuter) : defaultOuter;
        const float inner = unspecified ? outer * 0.5f : std::clamp(shape.innerRadius, 0.0f, outer);
        return PointInRing(point,
            {localRect.x + localRect.width * 0.5f, localRect.y + localRect.height * 0.5f}, inner, outer);
    }
    }
    return false;
}

} // namespace Engine::UI2D
