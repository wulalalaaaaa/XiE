#include "UI2D/Style/BasicUIInteractionStyleResolver.h"

#include "UI2D/Animation/UIAnimatedProperties.h"
#include "UI2D/Core/UIScene.h"

namespace Engine::UI2D {
namespace {
void Merge(UIStyleRuntimeProperties& target, const UIStyleProperties& value) {
    if (value.positionOffset) target.positionOffset = *value.positionOffset;
    if (value.scaleMultiplier) target.scaleMultiplier = *value.scaleMultiplier;
    if (value.rotationOffsetRadians) target.rotationOffsetRadians = *value.rotationOffsetRadians;
    if (value.opacityMultiplier) target.opacityMultiplier = *value.opacityMultiplier;
    if (value.colorMultiplier) target.colorMultiplier = *value.colorMultiplier;
}
void SetTransition(UIResolvedStyle& target, const UIStyleTransitions& transitions) {
    target.transitions = transitions;
}
bool EffectiveEnabled(const UIScene& scene, UINodeHandle node) {
    for (UINodeHandle current = node; current.IsValid();) {
        const UINodeRecord* record = scene.TryGet(current);
        if (!record || !record->enabled) return false;
        current = record->parent;
    }
    return true;
}

template <typename T>
void ApplyProperty(UIScene& scene, UINodeHandle node, UIAnimatedProperty property,
    const T& oldTarget, const T& newTarget, const UIStyleTransition& transition,
    bool immediate, IUIAnimator& animator, UIStyleUpdateResult& result) {
    if (oldTarget == newTarget) return;
    if (immediate || transition.durationSeconds <= 0.0) {
        if (SetAnimationChannelValue(scene, node, property,
            UIAnimationChannel::InteractionStyle, UIAnimationValue{newTarget})) {
            const UIDirtyFlags dirty = AnimatedPropertyDirtyFlags(property);
            result.transformInvalidated |= HasAny(dirty, UIDirtyFlags::Transform);
            result.visualInvalidated |= HasAny(dirty, UIDirtyFlags::Visual);
            result.hitTestInvalidated |= HasAny(dirty, UIDirtyFlags::HitTest);
        }
        return;
    }
    UIAnimationDesc desc;
    desc.node = node;
    desc.property = property;
    desc.channel = UIAnimationChannel::InteractionStyle;
    desc.to = UIAnimationValue{newTarget};
    desc.durationSeconds = transition.durationSeconds;
    desc.easing = transition.easing;
    desc.fillMode = UIAnimationFillMode::Forwards;
    desc.replaceMode = UIAnimationReplaceMode::Replace;
    desc.fromMode = UIAnimationFromMode::Current;
    const UIAnimationStartResult started = animator.Start(scene, desc);
    if (started.success) ++result.animationsStarted;
}
} // namespace

UIInteractionStateMask ResolveInteractionState(
    const UIScene& scene, const UIInteractionSnapshot& snapshot, UINodeHandle node) {
    if (!scene.TryGet(node)) return 0;
    UIInteractionStateMask result = 0;
    if (snapshot.IsHovered(node)) result |= ToMask(UIInteractionState::Hovered);
    if (snapshot.IsPressed(node)) result |= ToMask(UIInteractionState::Pressed);
    if (snapshot.IsFocused(node)) result |= ToMask(UIInteractionState::Focused);
    if (snapshot.HasFocusWithin(node)) result |= ToMask(UIInteractionState::FocusWithin);
    if (!EffectiveEnabled(scene, node)) result |= ToMask(UIInteractionState::Disabled);
    if (snapshot.HasPointerCapture(node)) result |= ToMask(UIInteractionState::Captured);
    if (!snapshot.IsWindowFocused()) result |= ToMask(UIInteractionState::WindowInactive);
    if (const UINodeRecord* record = scene.TryGet(node)) result |= record->style.explicitStates;
    return result;
}

bool MatchesStyleSelector(const UIStyleSelector& selector, UIStyleClassId styleClass,
    UIInteractionStateMask state) noexcept {
    return selector.styleClass == styleClass &&
        (state & selector.requiredStates) == selector.requiredStates &&
        (state & selector.forbiddenStates) == 0;
}

UIResolvedStyle ResolveNodeStyle(const UIResolvedStyleClass& styleClass,
    UIStyleClassId id, UIInteractionStateMask state) {
    UIResolvedStyle result;
    Merge(result, styleClass.baseStyle);
    result.transitions = styleClass.baseTransitions;
    for (const UIStyleRule& rule : styleClass.rules) {
        if (!MatchesStyleSelector(rule.selector, id, state)) continue;
        Merge(result, rule.properties);
        SetTransition(result, rule.transitions);
    }
    return result;
}

UIStyleUpdateResult BasicUIInteractionStyleResolver::Update(UIScene& scene,
    const UIStyleResolveContext& context, IUIAnimator& animator) {
    UIStyleUpdateResult result;
    std::vector<UINodeHandle> dirty = scene.ConsumeStyleDirtyNodes();
    if (dirty.empty()) return result;
    for (UINodeHandle handle : dirty) {
        UINodeRecord* node = scene.TryGet(handle);
        if (!node) continue;
        if (!node->style.reference.styleClass.IsValid() &&
            node->style.resolvedThemeRevision == 0) continue;
        ++result.checkedNodeCount;
        const UIResolvedStyleClass* styleClass =
            context.theme.TryGetStyleClass(node->style.reference.styleClass);
        if (!styleClass) {
            (void)animator.CancelAllForNodeChannel(scene, handle,
                UIAnimationChannel::InteractionStyle, UIAnimationCancelMode::RestoreBase);
            const UIStyleRuntimeProperties identity{};
            (void)SetAnimationChannelValue(scene, handle, UIAnimatedProperty::Position,
                UIAnimationChannel::InteractionStyle, identity.positionOffset);
            (void)SetAnimationChannelValue(scene, handle, UIAnimatedProperty::Scale,
                UIAnimationChannel::InteractionStyle, identity.scaleMultiplier);
            (void)SetAnimationChannelValue(scene, handle, UIAnimatedProperty::Rotation,
                UIAnimationChannel::InteractionStyle, identity.rotationOffsetRadians);
            (void)SetAnimationChannelValue(scene, handle, UIAnimatedProperty::Opacity,
                UIAnimationChannel::InteractionStyle, identity.opacityMultiplier);
            (void)SetAnimationChannelValue(scene, handle, UIAnimatedProperty::ColorMultiplier,
                UIAnimationChannel::InteractionStyle, identity.colorMultiplier);
            node->style.lastInteractionState = 0;
            node->style.lastResolvedTarget = {};
            node->style.resolvedThemeRevision = context.theme.Revision();
            continue;
        }
        const UIInteractionStateMask state = ResolveInteractionState(scene, context.interaction, handle);
        const UIResolvedStyle target = ResolveNodeStyle(
            *styleClass, node->style.reference.styleClass, state);
        if (node->style.resolvedThemeRevision == context.theme.Revision() &&
            node->style.lastInteractionState == state && node->style.lastResolvedTarget == target)
            continue;
        const bool initial = node->style.resolvedThemeRevision == 0;
        UIResolvedStyle old;
        static_cast<UIStyleRuntimeProperties&>(old) = node->style.runtimeProperties;
        const bool immediate = context.forceImmediate || initial;
        ApplyProperty(scene, handle, UIAnimatedProperty::Position, old.positionOffset,
            target.positionOffset, target.transitions.position, immediate, animator, result);
        ApplyProperty(scene, handle, UIAnimatedProperty::Scale, old.scaleMultiplier,
            target.scaleMultiplier, target.transitions.scale, immediate, animator, result);
        ApplyProperty(scene, handle, UIAnimatedProperty::Rotation, old.rotationOffsetRadians,
            target.rotationOffsetRadians, target.transitions.rotation, immediate, animator, result);
        ApplyProperty(scene, handle, UIAnimatedProperty::Opacity, old.opacityMultiplier,
            target.opacityMultiplier, target.transitions.opacity, immediate, animator, result);
        ApplyProperty(scene, handle, UIAnimatedProperty::ColorMultiplier, old.colorMultiplier,
            target.colorMultiplier, target.transitions.color, immediate, animator, result);
        node->style.lastInteractionState = state;
        node->style.lastResolvedTarget = target;
        node->style.resolvedThemeRevision = context.theme.Revision();
        ++node->style.styleRevision;
        ++result.changedNodeCount;
    }
    return result;
}

} // namespace Engine::UI2D
