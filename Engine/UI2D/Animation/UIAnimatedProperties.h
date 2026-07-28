#pragma once

#include "Foundation/Math/Types2D.h"
#include "UI2D/Animation/UIAnimationTypes.h"
#include "UI2D/Core/UIDirtyFlags.h"

#include <optional>

namespace Engine::UI2D {

class UIScene;
struct UINodeRecord;

struct UIAnimatedOverrides {
    std::optional<Engine::Vec2F> position;
    std::optional<Engine::Vec2F> visualSize;
    std::optional<Engine::Vec2F> scale;
    std::optional<float> rotationRadians;
    std::optional<float> opacity;
    std::optional<Engine::Color4f> colorMultiplier;
};

struct UIResolvedAnimatedProperties {
    Engine::Vec2F position{};
    Engine::Vec2F visualSize{};
    Engine::Vec2F scale{1.0f, 1.0f};
    float rotationRadians = 0.0f;
    float opacity = 1.0f;
    Engine::Color4f colorMultiplier{};
};

[[nodiscard]] UIResolvedAnimatedProperties ResolveAnimatedProperties(const UINodeRecord& node) noexcept;
[[nodiscard]] UIAnimationValue ResolveAnimatedProperty(
    const UINodeRecord& node, UIAnimatedProperty property) noexcept;
[[nodiscard]] UIAnimationValue ResolveAnimationChannelProperty(
    const UINodeRecord& node, UIAnimatedProperty property, UIAnimationChannel channel) noexcept;
[[nodiscard]] UIDirtyFlags AnimatedPropertyDirtyFlags(UIAnimatedProperty property) noexcept;
bool SetAnimatedOverride(UIScene& scene, UINodeHandle node, UIAnimatedProperty property,
    const UIAnimationValue& value);
bool ClearAnimatedOverride(UIScene& scene, UINodeHandle node, UIAnimatedProperty property);
bool SetAnimationChannelValue(UIScene& scene, UINodeHandle node, UIAnimatedProperty property,
    UIAnimationChannel channel, const UIAnimationValue& value);
bool ClearAnimationChannelValue(UIScene& scene, UINodeHandle node, UIAnimatedProperty property,
    UIAnimationChannel channel);
void UpdateAnimatedTransforms(UIScene& scene);

} // namespace Engine::UI2D
