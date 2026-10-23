#pragma once

#include "Core/GameApp.h"
#include "World2D.h"

namespace Test2D {

class CameraRuntime2D {
public:
    void SetFollowPlayer(bool followPlayer);
    void SetZoom(float zoom);
    void SetFallbackCenter(float x, float y);

    void Sync(const World2D& world, Engine::IRuntimeRender2D* runtimeRender2D);

private:
    bool m_FollowPlayer = true;
    float m_Zoom = 1.0f;
    Vec2 m_FallbackCenter{640.0f, 360.0f};
    bool m_WarnedMissingPlayer = false;
    bool m_WarnedInvalidZoom = false;
};

} // namespace Test2D
