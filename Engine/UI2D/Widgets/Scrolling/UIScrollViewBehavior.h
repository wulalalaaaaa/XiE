#pragma once

#include "UI2D/Core/UINodeHandle.h"
#include "UI2D/Widgets/Scrolling/UIScrollModel.h"

#include <array>
#include <cstddef>

namespace Engine::UI2D {

struct UIScrollViewState {
    UIScrollModel model;
    UINodeHandle viewportNode{};
    UIWidgetHandle contentHost{};
    UIWidgetHandle verticalScrollbar{};
    UIWidgetHandle horizontalScrollbar{};
    UIScrollDragState drag{};
    UISmoothScrollState smooth{};
    UIScrollInertiaState inertia{};
    std::array<UIScrollVelocitySample, 5> velocitySamples{};
    std::size_t velocitySampleCount = 0;
    std::size_t velocitySampleCursor = 0;
    std::uint64_t syncedLayoutRevision = 0;
    std::uint64_t appliedModelRevision = 0;
    Engine::Vec2F eventPreviousOffset{};
    UIScrollInputMode eventSource = UIScrollInputMode::Programmatic;
    bool changePending = false;
    bool completionPending = false;
    bool bringFocusedNodeIntoView = true;
    bool scrollingStyleApplied = false;
};

} // namespace Engine::UI2D
