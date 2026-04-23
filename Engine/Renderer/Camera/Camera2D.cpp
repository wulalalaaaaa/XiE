#include "Camera2D.h"

#include <algorithm>

namespace Engine {

void Camera2D::SetCanvas(const Canvas2DDesc& canvas) {
    m_Canvas = canvas;
}

void Camera2D::SetViewportSize(int width, int height) {
    m_ViewportWidth = (width > 0) ? width : 1;
    m_ViewportHeight = (height > 0) ? height : 1;
}

void Camera2D::SetPosition(float x, float y) {
    m_PositionX = x;
    m_PositionY = y;
}

void Camera2D::Move(float dx, float dy) {
    m_PositionX += dx;
    m_PositionY += dy;
}

void Camera2D::SetZoom(float zoom) {
    m_Zoom = std::clamp(zoom, 0.1f, 10.0f);
}

void Camera2D::AddZoom(float delta) {
    SetZoom(m_Zoom + delta);
}

std::array<float, 16> Camera2D::GetViewProjection() const {
    const float aspect = static_cast<float>(m_ViewportWidth) / static_cast<float>(m_ViewportHeight);

    float halfW = (m_Canvas.worldWidth * 0.5f) / m_Zoom;
    float halfH = (m_Canvas.worldHeight * 0.5f) / m_Zoom;

    if (m_Canvas.worldWidth <= 0.0f || m_Canvas.worldHeight <= 0.0f) {
        halfH = 360.0f / m_Zoom;
        halfW = halfH * aspect;
    }

    float left = m_PositionX - halfW;
    float right = m_PositionX + halfW;
    float bottom = m_PositionY - halfH;
    float top = m_PositionY + halfH;

    if (!m_Canvas.originAtCenter) {
        left += halfW;
        right += halfW;
        bottom += halfH;
        top += halfH;
    }

    return MakeOrtho(left, right, bottom, top);
}

std::array<float, 16> Camera2D::MakeOrtho(float left, float right, float bottom, float top) const {
    const float rl = right - left;
    const float tb = top - bottom;

    std::array<float, 16> m = {
        2.0f / rl, 0.0f,      0.0f, 0.0f,
        0.0f,      2.0f / tb, 0.0f, 0.0f,
        0.0f,      0.0f,     -1.0f, 0.0f,
        -(right + left) / rl,
        -(top + bottom) / tb,
        0.0f,
        1.0f
    };

    return m;
}

} // namespace Engine
