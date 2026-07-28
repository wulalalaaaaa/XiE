#pragma once

#include "Foundation/Handles/GenerationalHandle.h"
#include "Foundation/Math/Types2D.h"
#include "UI2D/Animation/UIAnimationTypes.h"

#include <cstdint>
#include <optional>

namespace Engine::UI2D {

struct UIStyleClassIdTag;
using UIStyleClassId = Engine::GenerationalHandle<UIStyleClassIdTag>;

enum class UIInteractionState : std::uint32_t {
    None = 0,
    Hovered = 1u << 0,
    Pressed = 1u << 1,
    Focused = 1u << 2,
    FocusWithin = 1u << 3,
    Disabled = 1u << 4,
    Captured = 1u << 5,
    WindowInactive = 1u << 6,
    Checked = 1u << 7,
    Scrolling = 1u << 8
};
using UIInteractionStateMask = std::uint32_t;
constexpr UIInteractionStateMask ToMask(UIInteractionState state) noexcept {
    return static_cast<UIInteractionStateMask>(state);
}
constexpr UIInteractionStateMask operator|(UIInteractionState lhs, UIInteractionState rhs) noexcept {
    return ToMask(lhs) | ToMask(rhs);
}
constexpr UIInteractionStateMask operator|(
    UIInteractionStateMask lhs, UIInteractionState rhs) noexcept { return lhs | ToMask(rhs); }
constexpr UIInteractionStateMask operator|(
    UIInteractionState lhs, UIInteractionStateMask rhs) noexcept { return ToMask(lhs) | rhs; }
constexpr UIInteractionStateMask operator&(
    UIInteractionStateMask lhs, UIInteractionState rhs) noexcept { return lhs & ToMask(rhs); }
inline UIInteractionStateMask& operator|=(
    UIInteractionStateMask& lhs, UIInteractionState rhs) noexcept { lhs |= ToMask(rhs); return lhs; }
constexpr bool HasInteractionState(
    UIInteractionStateMask mask, UIInteractionState state) noexcept {
    return (mask & ToMask(state)) != 0;
}

struct UIStyleTransition {
    double durationSeconds = 0.0;
    UIEasing easing = UIEasing::Linear;
    friend bool operator==(const UIStyleTransition&, const UIStyleTransition&) = default;
};
struct UIStyleTransitions {
    UIStyleTransition position;
    UIStyleTransition scale;
    UIStyleTransition rotation;
    UIStyleTransition opacity;
    UIStyleTransition color;
    friend bool operator==(const UIStyleTransitions&, const UIStyleTransitions&) = default;
};
struct UIStyleProperties {
    std::optional<Engine::Vec2F> positionOffset;
    std::optional<Engine::Vec2F> scaleMultiplier;
    std::optional<float> rotationOffsetRadians;
    std::optional<float> opacityMultiplier;
    std::optional<Engine::Color4f> colorMultiplier;
    friend bool operator==(const UIStyleProperties&, const UIStyleProperties&) = default;
};
struct UIStyleRuntimeProperties {
    Engine::Vec2F positionOffset{};
    Engine::Vec2F scaleMultiplier{1.0f, 1.0f};
    float rotationOffsetRadians = 0.0f;
    float opacityMultiplier = 1.0f;
    Engine::Color4f colorMultiplier{};
    friend bool operator==(const UIStyleRuntimeProperties&, const UIStyleRuntimeProperties&) = default;
};
struct UIResolvedStyle : UIStyleRuntimeProperties {
    UIStyleTransitions transitions;
    friend bool operator==(const UIResolvedStyle&, const UIResolvedStyle&) = default;
};
struct UIStyleReference {
    UIStyleClassId styleClass{};
    friend bool operator==(const UIStyleReference&, const UIStyleReference&) = default;
};
struct UINodeStyleState {
    UIStyleReference reference;
    UIInteractionStateMask lastInteractionState = 0;
    UIInteractionStateMask explicitStates = 0;
    UIResolvedStyle lastResolvedTarget{};
    UIStyleRuntimeProperties runtimeProperties{};
    std::uint64_t resolvedThemeRevision = 0;
    std::uint64_t styleRevision = 0;
};

} // namespace Engine::UI2D
