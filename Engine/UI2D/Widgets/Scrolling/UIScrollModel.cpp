#include "UI2D/Widgets/Scrolling/UIScrollModel.h"

#include <algorithm>
#include <cmath>

namespace Engine::UI2D {
namespace {
constexpr float kEpsilon = 1.0e-4f;
float FiniteNonNegative(float value) noexcept {
    return std::isfinite(value) ? std::max(0.0f, value) : 0.0f;
}
float SafePositive(float value, float fallback) noexcept {
    return std::isfinite(value) && value > 0.0f ? value : fallback;
}
bool Near(float a, float b) noexcept { return std::abs(a - b) <= kEpsilon; }
bool Near(Engine::Vec2F a, Engine::Vec2F b) noexcept { return Near(a.x, b.x) && Near(a.y, b.y); }
}

Engine::Vec2F UIScrollModel::NormalizeSize(Engine::Vec2F value) noexcept {
    return {FiniteNonNegative(value.x), FiniteNonNegative(value.y)};
}

UIScrollPolicy UIScrollModel::NormalizePolicy(UIScrollPolicy value) noexcept {
    value.wheelStep = SafePositive(value.wheelStep, 48.0f);
    value.pageStepRatio = std::clamp(std::isfinite(value.pageStepRatio) ? value.pageStepRatio : 0.9f,
        0.0f, 1.0f);
    value.dragThreshold = FiniteNonNegative(value.dragThreshold);
    value.inertiaDeceleration = SafePositive(value.inertiaDeceleration, 2600.0f);
    value.minimumInertiaVelocity = FiniteNonNegative(value.minimumInertiaVelocity);
    value.maximumVelocity = SafePositive(value.maximumVelocity, 6000.0f);
    value.minimumInertiaVelocity = std::min(value.minimumInertiaVelocity, value.maximumVelocity);
    value.smoothDurationSeconds = std::isfinite(value.smoothDurationSeconds) &&
        value.smoothDurationSeconds > 0.0 ? value.smoothDurationSeconds : 0.16;
    return value;
}

Engine::Vec2F UIScrollModel::MaximumOffset() const noexcept {
    return {
        m_Policy.horizontal == UIScrollMode::Disabled ? 0.0f :
            std::max(0.0f, m_ContentSize.x - m_ViewportSize.x),
        m_Policy.vertical == UIScrollMode::Disabled ? 0.0f :
            std::max(0.0f, m_ContentSize.y - m_ViewportSize.y)};
}

Engine::Vec2F UIScrollModel::ClampOffset(Engine::Vec2F value) const noexcept {
    const Engine::Vec2F maximum = MaximumOffset();
    value.x = std::isfinite(value.x) ? value.x : 0.0f;
    value.y = std::isfinite(value.y) ? value.y : 0.0f;
    return {std::clamp(value.x, 0.0f, maximum.x), std::clamp(value.y, 0.0f, maximum.y)};
}

bool UIScrollModel::RecomputeAfterBoundsChange() {
    const Engine::Vec2F next = ClampOffset(m_Offset);
    if (Near(next, m_Offset)) return false;
    m_Offset = next;
    return true;
}

void UIScrollModel::SetViewportSize(Engine::Vec2F size) {
    const Engine::Vec2F next = NormalizeSize(size);
    if (Near(next, m_ViewportSize)) return;
    m_ViewportSize = next;
    (void)RecomputeAfterBoundsChange();
    ++m_Revision;
}

void UIScrollModel::SetContentSize(Engine::Vec2F size) {
    const Engine::Vec2F next = NormalizeSize(size);
    if (Near(next, m_ContentSize)) return;
    m_ContentSize = next;
    (void)RecomputeAfterBoundsChange();
    ++m_Revision;
}

void UIScrollModel::SetPolicy(const UIScrollPolicy& policy) {
    const UIScrollPolicy next = NormalizePolicy(policy);
    if (next == m_Policy) return;
    m_Policy = next;
    (void)RecomputeAfterBoundsChange();
    ++m_Revision;
}

Engine::Vec2F UIScrollModel::NormalizedOffset() const noexcept {
    const Engine::Vec2F maximum = MaximumOffset();
    return {maximum.x > kEpsilon ? m_Offset.x / maximum.x : 0.0f,
        maximum.y > kEpsilon ? m_Offset.y / maximum.y : 0.0f};
}

bool UIScrollModel::SetOffset(Engine::Vec2F offset, UIScrollInputMode source) {
    const Engine::Vec2F next = ClampOffset(offset);
    if (Near(next, m_Offset)) return false;
    m_Offset = next;
    m_LastSource = source;
    ++m_Revision;
    return true;
}

bool UIScrollModel::ScrollBy(Engine::Vec2F delta, UIScrollInputMode source) {
    if (!std::isfinite(delta.x)) delta.x = 0.0f;
    if (!std::isfinite(delta.y)) delta.y = 0.0f;
    return SetOffset({m_Offset.x + delta.x, m_Offset.y + delta.y}, source);
}

bool UIScrollModel::SetNormalizedOffset(Engine::Vec2F normalized) {
    normalized.x = std::clamp(std::isfinite(normalized.x) ? normalized.x : 0.0f, 0.0f, 1.0f);
    normalized.y = std::clamp(std::isfinite(normalized.y) ? normalized.y : 0.0f, 0.0f, 1.0f);
    const Engine::Vec2F maximum = MaximumOffset();
    return SetOffset({normalized.x * maximum.x, normalized.y * maximum.y},
        UIScrollInputMode::Scrollbar);
}

bool UIScrollModel::CanScroll(UIScrollAxis axis) const noexcept {
    const Engine::Vec2F maximum = MaximumOffset();
    return axis == UIScrollAxis::Horizontal ? maximum.x > kEpsilon : maximum.y > kEpsilon;
}
bool UIScrollModel::IsAtStart(UIScrollAxis axis) const noexcept {
    return axis == UIScrollAxis::Horizontal ? m_Offset.x <= kEpsilon : m_Offset.y <= kEpsilon;
}
bool UIScrollModel::IsAtEnd(UIScrollAxis axis) const noexcept {
    const Engine::Vec2F maximum = MaximumOffset();
    return axis == UIScrollAxis::Horizontal ? m_Offset.x >= maximum.x - kEpsilon :
        m_Offset.y >= maximum.y - kEpsilon;
}
float UIScrollModel::PageStep(UIScrollAxis axis) const noexcept {
    const float viewport = axis == UIScrollAxis::Horizontal ? m_ViewportSize.x : m_ViewportSize.y;
    return viewport * m_Policy.pageStepRatio;
}

} // namespace Engine::UI2D
