#pragma once

#include "UI2D/Core/UINodeHandle.h"
#include "UI2D/Widgets/Scrolling/UIScrollTypes.h"

namespace Engine::UI2D {

struct UIScrollbarState {
    UIScrollAxis axis = UIScrollAxis::Vertical;
    UIWidgetHandle boundScrollView{};
    UINodeHandle thumbNode{};
    float minimumThumbLength = 18.0f;
    float trackLength = 0.0f;
    float thumbLength = 0.0f;
    float thumbPosition = 0.0f;
    float thumbTravel = 0.0f;
    std::uint64_t syncedModelRevision = 0;
    Engine::PointerId pointerId = 0;
    Engine::Vec2F dragStartScenePosition{};
    float dragStartNormalizedOffset = 0.0f;
    bool dragging = false;
};

} // namespace Engine::UI2D
