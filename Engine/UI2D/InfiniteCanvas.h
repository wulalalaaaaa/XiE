#pragma once

#include "Foundation/Math/Types2D.h"

#include <algorithm>
#include <cmath>

namespace Engine::UI2D {

// Camera state for a world-space canvas. Camera coordinates are stored as
// doubles so an application can pan over a large workspace without making
// the screen transform accumulate float error.
struct CanvasCamera2D {
    double centerX = 0.0;
    double centerY = 0.0;
    double zoom = 1.0;
    Engine::Vec2F viewportSize{1280.0f, 720.0f};

    [[nodiscard]] Engine::Vec2F WorldToScreen(Engine::Vec2F world) const noexcept;
    [[nodiscard]] Engine::Vec2F ScreenToWorld(Engine::Vec2F screen) const noexcept;
    void SetViewportSize(Engine::Vec2F size) noexcept;
    void SetZoom(double value) noexcept;
    void PanScreenDelta(Engine::Vec2F delta) noexcept;
    void ZoomAroundCursor(Engine::Vec2F screenPoint, double factor) noexcept;
};

class InfiniteCanvas {
public:
    [[nodiscard]] const CanvasCamera2D& Camera() const noexcept { return m_Camera; }
    [[nodiscard]] CanvasCamera2D& Camera() noexcept { return m_Camera; }

    void SetViewportSize(Engine::Vec2F size) noexcept { m_Camera.SetViewportSize(size); }
    void Pan(Engine::Vec2F screenDelta) noexcept { m_Camera.PanScreenDelta(screenDelta); }
    void ZoomAt(Engine::Vec2F screenPoint, double factor) noexcept {
        m_Camera.ZoomAroundCursor(screenPoint, factor);
    }

    [[nodiscard]] Engine::Vec2F WorldToScreen(Engine::Vec2F world) const noexcept {
        return m_Camera.WorldToScreen(world);
    }
    [[nodiscard]] Engine::Vec2F ScreenToWorld(Engine::Vec2F screen) const noexcept {
        return m_Camera.ScreenToWorld(screen);
    }

private:
    CanvasCamera2D m_Camera{};
};

} // namespace Engine::UI2D
