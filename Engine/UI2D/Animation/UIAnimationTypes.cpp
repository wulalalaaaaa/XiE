#include "UI2D/Animation/UIAnimationTypes.h"

#include <algorithm>
#include <cmath>

namespace Engine::UI2D {

float EvaluateEasing(UIEasing easing, float value) noexcept {
    const float t = std::clamp(std::isfinite(value) ? value : 0.0f, 0.0f, 1.0f);
    switch (easing) {
    case UIEasing::EaseInQuad: return t * t;
    case UIEasing::EaseOutQuad: return t * (2.0f - t);
    case UIEasing::EaseInOutQuad: return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) * 0.5f;
    case UIEasing::EaseInCubic: return t * t * t;
    case UIEasing::EaseOutCubic: return 1.0f - std::pow(1.0f - t, 3.0f);
    case UIEasing::EaseInOutCubic: return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
    case UIEasing::SmoothStep: return t * t * (3.0f - 2.0f * t);
    case UIEasing::Linear: default: return t;
    }
}

UIAnimationValue InterpolateAnimationValue(const UIAnimationValue& from, const UIAnimationValue& to, float value) {
    const float t = std::clamp(std::isfinite(value) ? value : 0.0f, 0.0f, 1.0f);
    if (from.index() != to.index()) return from;
    if (const float* a = std::get_if<float>(&from)) {
        return *a + (std::get<float>(to) - *a) * t;
    }
    if (const Engine::Vec2F* a = std::get_if<Engine::Vec2F>(&from)) {
        const Engine::Vec2F b = std::get<Engine::Vec2F>(to);
        return Engine::Vec2F{a->x + (b.x - a->x) * t, a->y + (b.y - a->y) * t};
    }
    const Engine::Color4f a = std::get<Engine::Color4f>(from);
    const Engine::Color4f b = std::get<Engine::Color4f>(to);
    return Engine::Color4f{
        a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t,
        a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t};
}

bool IsAnimationValueCompatible(UIAnimatedProperty property, const UIAnimationValue& value) noexcept {
    switch (property) {
    case UIAnimatedProperty::Position:
    case UIAnimatedProperty::VisualSize:
    case UIAnimatedProperty::Scale: return std::holds_alternative<Engine::Vec2F>(value);
    case UIAnimatedProperty::Rotation:
    case UIAnimatedProperty::Opacity: return std::holds_alternative<float>(value);
    case UIAnimatedProperty::ColorMultiplier: return std::holds_alternative<Engine::Color4f>(value);
    }
    return false;
}

} // namespace Engine::UI2D
