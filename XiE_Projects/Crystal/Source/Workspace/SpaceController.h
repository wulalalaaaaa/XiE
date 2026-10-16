#pragma once

#include "Model/CrystalModels.h"
#include "UI2D/InfiniteCanvas.h"

namespace Crystal {

class SpaceController {
public:
    void Load(SpaceModel space);
    [[nodiscard]] const SpaceModel& Current() const noexcept { return m_Current; }
    [[nodiscard]] Engine::UI2D::InfiniteCanvas& Canvas() noexcept { return m_Canvas; }
    [[nodiscard]] const Engine::UI2D::InfiniteCanvas& Canvas() const noexcept { return m_Canvas; }

    void SetViewport(Engine::Vec2F size) noexcept { m_Canvas.SetViewportSize(size); }
    void SyncModelFromCamera() noexcept;

private:
    SpaceModel m_Current;
    Engine::UI2D::InfiniteCanvas m_Canvas;
};

} // namespace Crystal
