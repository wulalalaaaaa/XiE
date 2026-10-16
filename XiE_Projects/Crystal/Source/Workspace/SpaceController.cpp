#include "Workspace/SpaceController.h"

#include <utility>

namespace Crystal {

void SpaceController::Load(SpaceModel space) {
    m_Current = std::move(space);
    auto& camera = m_Canvas.Camera();
    camera.centerX = m_Current.cameraX;
    camera.centerY = m_Current.cameraY;
    camera.SetZoom(m_Current.zoom);
}

void SpaceController::SyncModelFromCamera() noexcept {
    const auto& camera = m_Canvas.Camera();
    m_Current.cameraX = camera.centerX;
    m_Current.cameraY = camera.centerY;
    m_Current.zoom = camera.zoom;
}

} // namespace Crystal
