#pragma once

#include "UI2D/Animation/UIAnimationTypes.h"

namespace Engine::UI2D {

struct UIAnimationState {
    UIAnimationHandle handle{};
    UINodeHandle node{};
    UIAnimatedProperty property = UIAnimatedProperty::Opacity;
    UIAnimationPlaybackState playback = UIAnimationPlaybackState::Queued;
    double elapsedSeconds = 0.0;
    std::uint32_t completedIterations = 0;
    std::uint64_t creationOrder = 0;
};

} // namespace Engine::UI2D
