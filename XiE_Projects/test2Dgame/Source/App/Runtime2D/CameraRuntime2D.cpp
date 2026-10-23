#include "CameraRuntime2D.h"

#include "Core/Log.h"

#include <cmath>
#include <string>

namespace Test2D {

void CameraRuntime2D::SetFollowPlayer(bool followPlayer) {
    m_FollowPlayer = followPlayer;
}

void CameraRuntime2D::SetZoom(float zoom) {
    m_Zoom = zoom;
}

void CameraRuntime2D::SetFallbackCenter(float x, float y) {
    m_FallbackCenter = {x, y};
}

void CameraRuntime2D::Sync(const World2D& world, Engine::IRuntimeRender2D* runtimeRender2D) {
    if (runtimeRender2D == nullptr) {
        return;
    }

    float zoom = m_Zoom;
    if (!std::isfinite(zoom) || zoom <= 0.0f) {
        if (!m_WarnedInvalidZoom) {
            XLOG_WARN("CameraRuntime2D: invalid zoom, falling back to 1.0");
            m_WarnedInvalidZoom = true;
        }
        zoom = 1.0f;
    }

    Vec2 center = m_FallbackCenter;
    if (m_FollowPlayer) {
        const Entity2D* player = world.FindEntity(world.GetPlayer());
        if (player != nullptr) {
            center = player->transform.position;
            m_WarnedMissingPlayer = false;
        } else if (!m_WarnedMissingPlayer) {
            XLOG_WARN("CameraRuntime2D: player entity missing, using fallback camera center");
            m_WarnedMissingPlayer = true;
        }
    }

    runtimeRender2D->SetCamera2D(center.x, center.y, zoom);
}

} // namespace Test2D
