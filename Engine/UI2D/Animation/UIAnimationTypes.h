#pragma once

#include "Foundation/Handles/GenerationalHandle.h"
#include "Foundation/Math/Types2D.h"
#include "UI2D/Core/UINodeHandle.h"

#include <cstdint>
#include <functional>
#include <string>
#include <variant>

namespace Engine::UI2D {

enum class UIAnimatedProperty { Position, VisualSize, Scale, Rotation, Opacity, ColorMultiplier };
enum class UIAnimationChannel { Application, InteractionStyle };
enum class UIEasing {
    Linear, EaseInQuad, EaseOutQuad, EaseInOutQuad,
    EaseInCubic, EaseOutCubic, EaseInOutCubic, SmoothStep
};
enum class UIAnimationDirection { Normal, Reverse, Alternate, AlternateReverse };
enum class UIAnimationFillMode { None, Forwards, Backwards, Both };
enum class UIAnimationReplaceMode { Reject, Replace, Queue };
enum class UIAnimationFromMode { Explicit, Current };
enum class UIAnimationCancelMode { RestoreBase, KeepCurrent };
enum class UIAnimationPlaybackState { Delayed, Running, Paused, Completed, Canceled, Queued };

struct UIAnimationHandleTag;
using UIAnimationHandle = Engine::GenerationalHandle<UIAnimationHandleTag>;
using UIAnimationValue = std::variant<float, Engine::Vec2F, Engine::Color4f>;
using UIAnimationCompletionCallback = std::function<void(UIAnimationHandle, UINodeHandle)>;
using UIAnimationCancelCallback = std::function<void(UIAnimationHandle, UINodeHandle)>;

struct UIAnimationDesc {
    UINodeHandle node{};
    UIAnimatedProperty property = UIAnimatedProperty::Opacity;
    UIAnimationChannel channel = UIAnimationChannel::Application;
    UIAnimationValue from = 0.0f;
    UIAnimationValue to = 1.0f;
    double delaySeconds = 0.0;
    double durationSeconds = 0.2;
    std::uint32_t iterationCount = 1;
    bool infinite = false;
    UIEasing easing = UIEasing::Linear;
    UIAnimationDirection direction = UIAnimationDirection::Normal;
    UIAnimationFillMode fillMode = UIAnimationFillMode::None;
    UIAnimationReplaceMode replaceMode = UIAnimationReplaceMode::Replace;
    UIAnimationFromMode fromMode = UIAnimationFromMode::Explicit;
    UIAnimationCompletionCallback onCompleted{};
    UIAnimationCancelCallback onCanceled{};
};

struct UIAnimationStartResult {
    bool success = false;
    UIAnimationHandle handle{};
    std::string error;
};

struct UIAnimationUpdateResult {
    std::uint32_t visitedAnimationCount = 0;
    std::uint32_t updatedCount = 0;
    std::uint32_t completedCount = 0;
    std::uint32_t canceledCount = 0;
    bool transformInvalidated = false;
    bool visualInvalidated = false;
    bool hitTestInvalidated = false;
    bool hasActiveAnimations = false;
    bool reentrantUpdateRejected = false;
};

[[nodiscard]] float EvaluateEasing(UIEasing easing, float normalizedTime) noexcept;
[[nodiscard]] UIAnimationValue InterpolateAnimationValue(
    const UIAnimationValue& from, const UIAnimationValue& to, float normalizedTime);
[[nodiscard]] bool IsAnimationValueCompatible(
    UIAnimatedProperty property, const UIAnimationValue& value) noexcept;

} // namespace Engine::UI2D
