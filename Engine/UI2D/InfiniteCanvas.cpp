#include "UI2D/InfiniteCanvas.h"

#include <limits>

namespace Engine::UI2D {
namespace {

constexpr double kSmallestUsefulZoom = 1.0e-6;
constexpr double kLargestUsefulZoom = 1.0e6;

double SafeZoom(double value) noexcept {
    if (!std::isfinite(value)) return 1.0;
    return std::clamp(value, kSmallestUsefulZoom, kLargestUsefulZoom);
}

} // namespace

Engine::Vec2F CanvasCamera2D::WorldToScreen(Engine::Vec2F world) const noexcept {
    const double safeZoom = SafeZoom(zoom);
    const double halfWidth = static_cast<double>(viewportSize.x) * 0.5;
    const double halfHeight = static_cast<double>(viewportSize.y) * 0.5;
    return {
        static_cast<float>((static_cast<double>(world.x) - centerX) * safeZoom + halfWidth),
        static_cast<float>((static_cast<double>(world.y) - centerY) * safeZoom + halfHeight)};
}

Engine::Vec2F CanvasCamera2D::ScreenToWorld(Engine::Vec2F screen) const noexcept {
    const double safeZoom = SafeZoom(zoom);
    const double halfWidth = static_cast<double>(viewportSize.x) * 0.5;
    const double halfHeight = static_cast<double>(viewportSize.y) * 0.5;
    return {
        static_cast<float>(centerX + (static_cast<double>(screen.x) - halfWidth) / safeZoom),
        static_cast<float>(centerY + (static_cast<double>(screen.y) - halfHeight) / safeZoom)};
}

void CanvasCamera2D::SetViewportSize(Engine::Vec2F size) noexcept {
    viewportSize.x = std::max(0.0f, size.x);
    viewportSize.y = std::max(0.0f, size.y);
}

void CanvasCamera2D::SetZoom(double value) noexcept { zoom = SafeZoom(value); }

void CanvasCamera2D::PanScreenDelta(Engine::Vec2F delta) noexcept {
    const double safeZoom = SafeZoom(zoom);
    centerX -= static_cast<double>(delta.x) / safeZoom;
    centerY -= static_cast<double>(delta.y) / safeZoom;
}

void CanvasCamera2D::ZoomAroundCursor(Engine::Vec2F screenPoint, double factor) noexcept {
    const Engine::Vec2F worldBefore = ScreenToWorld(screenPoint);
    SetZoom(SafeZoom(zoom) * (std::isfinite(factor) ? factor : 1.0));
    const double safeZoom = SafeZoom(zoom);
    const double halfWidth = static_cast<double>(viewportSize.x) * 0.5;
    const double halfHeight = static_cast<double>(viewportSize.y) * 0.5;
    centerX = static_cast<double>(worldBefore.x) -
        (static_cast<double>(screenPoint.x) - halfWidth) / safeZoom;
    centerY = static_cast<double>(worldBefore.y) -
        (static_cast<double>(screenPoint.y) - halfHeight) / safeZoom;
}

} // namespace Engine::UI2D
