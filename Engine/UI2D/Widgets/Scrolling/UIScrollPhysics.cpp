#include "UI2D/Widgets/Scrolling/UIScrollPhysics.h"

#include "UI2D/Animation/UIAnimationTypes.h"

#include <algorithm>
#include <cmath>

namespace Engine::UI2D {
namespace {
float Decelerate(float value, float amount) noexcept {
    if (value > 0.0f) return std::max(0.0f, value - amount);
    if (value < 0.0f) return std::min(0.0f, value + amount);
    return 0.0f;
}
bool ValidDelta(double value) noexcept { return std::isfinite(value) && value > 0.0; }
}

UIScrollPhysicsResult UIScrollPhysics::AdvanceSmooth(
    UIScrollModel& model, UISmoothScrollState& state, double deltaSeconds) {
    UIScrollPhysicsResult result;
    if (!state.active || !ValidDelta(deltaSeconds)) return result;
    state.elapsedSeconds += deltaSeconds;
    const double duration = std::max(1.0e-6, state.durationSeconds);
    const float progress = static_cast<float>(std::clamp(state.elapsedSeconds / duration, 0.0, 1.0));
    const float eased = EvaluateEasing(UIEasing::EaseOutCubic, progress);
    const Engine::Vec2F value{
        state.startOffset.x + (state.targetOffset.x - state.startOffset.x) * eased,
        state.startOffset.y + (state.targetOffset.y - state.startOffset.y) * eased};
    result.offsetChanged = model.SetOffset(value, state.source);
    if (progress >= 1.0f ||
        (model.Offset() == model.MaximumOffset() && state.targetOffset == model.MaximumOffset())) {
        state.active = false;
        result.completed = true;
    }
    return result;
}

UIScrollPhysicsResult UIScrollPhysics::AdvanceInertia(
    UIScrollModel& model, UIScrollInertiaState& state,
    const UIScrollPolicy& policy, double deltaSeconds) {
    UIScrollPhysicsResult result;
    if (!state.active || !ValidDelta(deltaSeconds)) return result;
    const float dt = static_cast<float>(std::min(deltaSeconds, 0.1));
    const Engine::Vec2F before = model.Offset();
    result.offsetChanged = model.ScrollBy({state.velocity.x * dt, state.velocity.y * dt},
        UIScrollInputMode::PointerDrag);
    const Engine::Vec2F after = model.Offset();
    if ((state.velocity.x < 0.0f && after.x <= 0.0f) ||
        (state.velocity.x > 0.0f && after.x >= model.MaximumOffset().x) || after.x == before.x)
        state.velocity.x = 0.0f;
    if ((state.velocity.y < 0.0f && after.y <= 0.0f) ||
        (state.velocity.y > 0.0f && after.y >= model.MaximumOffset().y) || after.y == before.y)
        state.velocity.y = 0.0f;
    const float deceleration = std::max(0.0f, policy.inertiaDeceleration) * dt;
    state.velocity.x = Decelerate(state.velocity.x, deceleration);
    state.velocity.y = Decelerate(state.velocity.y, deceleration);
    if (std::abs(state.velocity.x) < policy.minimumInertiaVelocity) state.velocity.x = 0.0f;
    if (std::abs(state.velocity.y) < policy.minimumInertiaVelocity) state.velocity.y = 0.0f;
    if (state.velocity.x == 0.0f && state.velocity.y == 0.0f) {
        state.active = false;
        result.completed = true;
    }
    return result;
}

} // namespace Engine::UI2D
