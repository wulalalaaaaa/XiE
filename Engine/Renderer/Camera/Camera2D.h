#pragma once

#include "Renderer/Scene/Canvas2DDesc.h"

#include <array>

namespace Engine {

class Camera2D {
public:
    void SetCanvas(const Canvas2DDesc& canvas);
    void SetViewportSize(int width, int height);

    void SetPosition(float x, float y);
    void Move(float dx, float dy);

    void SetZoom(float zoom);
    void AddZoom(float delta);

    std::array<float, 16> GetViewProjection() const;

private:
    std::array<float, 16> MakeOrtho(float left, float right, float bottom, float top) const;

private:
    Canvas2DDesc m_Canvas{};
    int m_ViewportWidth = 1280;
    int m_ViewportHeight = 720;

    float m_PositionX = 0.0f;
    float m_PositionY = 0.0f;
    float m_Zoom = 1.0f;
};

} // namespace Engine
