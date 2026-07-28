#pragma once

#include "UI2D/Animation/UIAnimationTypes.h"
#include "UI2D/Layout/UILayoutResult.h"
#include "UI2D/Input/UIEvent.h"
#include "UI2D/Runtime/UIFrameActivity.h"
#include "UI2D/Style/IUIInteractionStyleResolver.h"
#include "UI2D/Widgets/Core/UIWidgetTypes.h"

#include <cstddef>

namespace Engine::UI2D {

struct UI2DUpdateResult {
    UIFrameActivity activity{};
    std::size_t consumedInputCount = 0;
    std::size_t appliedMutationCount = 0;
    std::size_t mutationErrorCount = 0;
    UIInputDispatchResult inputDispatch{};
    UIAnimationUpdateResult animation{};
    UIStyleUpdateResult style{};
    UIWidgetUpdateResult widgets{};
    bool layoutUpdated = false;
    bool layoutFailed = false;
    UILayoutError layoutError = UILayoutError::None;
    bool drawListRebuilt = false;
};

} // namespace Engine::UI2D
