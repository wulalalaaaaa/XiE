#pragma once

#include "UI2D/Widgets/Scrolling/UIScrollModel.h"

namespace Engine::UI2D {

struct UIScrollPhysicsResult {
    bool offsetChanged = false;
    bool completed = false;
};

class UIScrollPhysics {
public:
    static UIScrollPhysicsResult AdvanceSmooth(
        UIScrollModel& model, UISmoothScrollState& state, double deltaSeconds);
    static UIScrollPhysicsResult AdvanceInertia(
        UIScrollModel& model, UIScrollInertiaState& state,
        const UIScrollPolicy& policy, double deltaSeconds);
};

} // namespace Engine::UI2D
