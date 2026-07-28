#include "UI2D/Animation/UIAnimatedProperties.h"

#include "Foundation/Math/Matrix3.h"
#include "UI2D/Core/UINodeRecord.h"
#include "UI2D/Core/UIScene.h"

#include <algorithm>
#include <cmath>

namespace Engine::UI2D {
namespace {

float Finite(float value, float fallback = 0.0f) noexcept { return std::isfinite(value) ? value : fallback; }
Engine::Vec2F Finite(Engine::Vec2F value, Engine::Vec2F fallback = {}) noexcept {
    return {Finite(value.x, fallback.x), Finite(value.y, fallback.y)};
}
Engine::Color4f SafeColor(Engine::Color4f value) noexcept {
    return {std::clamp(Finite(value.r, 1.0f), 0.0f, 1.0f),
        std::clamp(Finite(value.g, 1.0f), 0.0f, 1.0f),
        std::clamp(Finite(value.b, 1.0f), 0.0f, 1.0f),
        std::clamp(Finite(value.a, 1.0f), 0.0f, 1.0f)};
}
Engine::RectF TransformBounds(const Engine::Mat3F& transform, Engine::Vec2F size) noexcept {
    const Engine::Vec2F points[] = {
        Engine::TransformPoint(transform, {}), Engine::TransformPoint(transform, {size.x, 0.0f}),
        Engine::TransformPoint(transform, {0.0f, size.y}), Engine::TransformPoint(transform, size)};
    float minX = points[0].x, maxX = points[0].x, minY = points[0].y, maxY = points[0].y;
    for (Engine::Vec2F point : points) {
        minX = std::min(minX, point.x); maxX = std::max(maxX, point.x);
        minY = std::min(minY, point.y); maxY = std::max(maxY, point.y);
    }
    return {minX, minY, std::max(0.0f, maxX - minX), std::max(0.0f, maxY - minY)};
}

} // namespace

UIResolvedAnimatedProperties ResolveAnimatedProperties(const UINodeRecord& node) noexcept {
    UIResolvedAnimatedProperties result;
    const UIStyleRuntimeProperties& style = node.style.runtimeProperties;
    const Engine::Vec2F applicationPosition = Finite(node.animated.position.value_or(node.transform.position));
    const Engine::Vec2F stylePosition = Finite(style.positionOffset);
    const Engine::Vec2F widgetPosition = Finite(node.widgetRuntime.positionOffset);
    result.position = {applicationPosition.x + stylePosition.x + widgetPosition.x,
        applicationPosition.y + stylePosition.y + widgetPosition.y};
    const Engine::Vec2F baseSize = node.layoutState.arrangeValid
        ? Engine::Vec2F{node.layoutState.arrangedRect.width, node.layoutState.arrangedRect.height}
        : node.transform.size;
    result.visualSize = Finite(node.animated.visualSize.value_or(baseSize));
    result.visualSize.x = std::max(0.0f, result.visualSize.x);
    result.visualSize.y = std::max(0.0f, result.visualSize.y);
    const Engine::Vec2F applicationScale = Finite(
        node.animated.scale.value_or(node.transform.scale), {1.0f, 1.0f});
    const Engine::Vec2F styleScale = Finite(style.scaleMultiplier, {1.0f, 1.0f});
    result.scale = {applicationScale.x * styleScale.x, applicationScale.y * styleScale.y};
    result.rotationRadians = Finite(node.animated.rotationRadians.value_or(node.transform.rotationRadians)) +
        Finite(style.rotationOffsetRadians);
    result.opacity = std::clamp(Finite(node.animated.opacity.value_or(node.opacity), 1.0f) *
        Finite(style.opacityMultiplier, 1.0f), 0.0f, 1.0f);
    const Engine::Color4f applicationColor = SafeColor(
        node.animated.colorMultiplier.value_or(Engine::Color4f{}));
    const Engine::Color4f styleColor = SafeColor(style.colorMultiplier);
    result.colorMultiplier = SafeColor({applicationColor.r * styleColor.r,
        applicationColor.g * styleColor.g, applicationColor.b * styleColor.b,
        applicationColor.a * styleColor.a});
    return result;
}

UIAnimationValue ResolveAnimatedProperty(const UINodeRecord& node, UIAnimatedProperty property) noexcept {
    const UIResolvedAnimatedProperties resolved = ResolveAnimatedProperties(node);
    switch (property) {
    case UIAnimatedProperty::Position: return resolved.position;
    case UIAnimatedProperty::VisualSize: return resolved.visualSize;
    case UIAnimatedProperty::Scale: return resolved.scale;
    case UIAnimatedProperty::Rotation: return resolved.rotationRadians;
    case UIAnimatedProperty::Opacity: return resolved.opacity;
    case UIAnimatedProperty::ColorMultiplier: return resolved.colorMultiplier;
    }
    return 0.0f;
}

UIAnimationValue ResolveAnimationChannelProperty(const UINodeRecord& node,
    UIAnimatedProperty property, UIAnimationChannel channel) noexcept {
    if (channel == UIAnimationChannel::Application) {
        switch (property) {
        case UIAnimatedProperty::Position:
            return Finite(node.animated.position.value_or(node.transform.position));
        case UIAnimatedProperty::VisualSize: {
            const Engine::Vec2F base = node.layoutState.arrangeValid
                ? Engine::Vec2F{node.layoutState.arrangedRect.width, node.layoutState.arrangedRect.height}
                : node.transform.size;
            return Finite(node.animated.visualSize.value_or(base));
        }
        case UIAnimatedProperty::Scale:
            return Finite(node.animated.scale.value_or(node.transform.scale), {1.0f, 1.0f});
        case UIAnimatedProperty::Rotation:
            return Finite(node.animated.rotationRadians.value_or(node.transform.rotationRadians));
        case UIAnimatedProperty::Opacity:
            return std::clamp(Finite(node.animated.opacity.value_or(node.opacity), 1.0f), 0.0f, 1.0f);
        case UIAnimatedProperty::ColorMultiplier:
            return SafeColor(node.animated.colorMultiplier.value_or(Engine::Color4f{}));
        }
    }
    const UIStyleRuntimeProperties& style = node.style.runtimeProperties;
    switch (property) {
    case UIAnimatedProperty::Position: return style.positionOffset;
    case UIAnimatedProperty::Scale: return style.scaleMultiplier;
    case UIAnimatedProperty::Rotation: return style.rotationOffsetRadians;
    case UIAnimatedProperty::Opacity: return style.opacityMultiplier;
    case UIAnimatedProperty::ColorMultiplier: return style.colorMultiplier;
    case UIAnimatedProperty::VisualSize: return Engine::Vec2F{};
    }
    return 0.0f;
}

UIDirtyFlags AnimatedPropertyDirtyFlags(UIAnimatedProperty property) noexcept {
    switch (property) {
    case UIAnimatedProperty::Position:
    case UIAnimatedProperty::Scale:
    case UIAnimatedProperty::Rotation:
        return UIDirtyFlags::Transform | UIDirtyFlags::Visual | UIDirtyFlags::HitTest;
    case UIAnimatedProperty::VisualSize:
        return UIDirtyFlags::Transform | UIDirtyFlags::Visual | UIDirtyFlags::HitTest;
    case UIAnimatedProperty::Opacity:
    case UIAnimatedProperty::ColorMultiplier:
        return UIDirtyFlags::Visual;
    }
    return UIDirtyFlags::None;
}

bool SetAnimatedOverride(UIScene& scene, UINodeHandle handle, UIAnimatedProperty property,
    const UIAnimationValue& value) {
    UINodeRecord* node = scene.TryGet(handle);
    if (!node || !IsAnimationValueCompatible(property, value)) return false;
    UIAnimationValue sanitized = value;
    switch (property) {
    case UIAnimatedProperty::Position: sanitized = Finite(std::get<Engine::Vec2F>(value)); break;
    case UIAnimatedProperty::VisualSize: {
        const Engine::Vec2F size = Finite(std::get<Engine::Vec2F>(value));
        sanitized = Engine::Vec2F{std::max(0.0f, size.x), std::max(0.0f, size.y)};
        break;
    }
    case UIAnimatedProperty::Scale: sanitized = Finite(std::get<Engine::Vec2F>(value), {1.0f, 1.0f}); break;
    case UIAnimatedProperty::Rotation: sanitized = Finite(std::get<float>(value)); break;
    case UIAnimatedProperty::Opacity: sanitized = std::clamp(Finite(std::get<float>(value)), 0.0f, 1.0f); break;
    case UIAnimatedProperty::ColorMultiplier: sanitized = SafeColor(std::get<Engine::Color4f>(value)); break;
    }
    if (ResolveAnimatedProperty(*node, property) == sanitized) return false;
    switch (property) {
    case UIAnimatedProperty::Position: node->animated.position = std::get<Engine::Vec2F>(sanitized); break;
    case UIAnimatedProperty::VisualSize: node->animated.visualSize = std::get<Engine::Vec2F>(sanitized); break;
    case UIAnimatedProperty::Scale: node->animated.scale = std::get<Engine::Vec2F>(sanitized); break;
    case UIAnimatedProperty::Rotation: node->animated.rotationRadians = std::get<float>(sanitized); break;
    case UIAnimatedProperty::Opacity: node->animated.opacity = std::get<float>(sanitized); break;
    case UIAnimatedProperty::ColorMultiplier: node->animated.colorMultiplier = std::get<Engine::Color4f>(sanitized); break;
    }
    if (!node->visible || (node->layoutState.arrangeValid && !node->layoutState.effectiveVisible)) return true;
    return scene.MarkDirty(handle, AnimatedPropertyDirtyFlags(property));
}

bool ClearAnimatedOverride(UIScene& scene, UINodeHandle handle, UIAnimatedProperty property) {
    UINodeRecord* node = scene.TryGet(handle);
    if (!node) return false;
    bool had = false;
    switch (property) {
    case UIAnimatedProperty::Position: had = node->animated.position.has_value(); node->animated.position.reset(); break;
    case UIAnimatedProperty::VisualSize: had = node->animated.visualSize.has_value(); node->animated.visualSize.reset(); break;
    case UIAnimatedProperty::Scale: had = node->animated.scale.has_value(); node->animated.scale.reset(); break;
    case UIAnimatedProperty::Rotation: had = node->animated.rotationRadians.has_value(); node->animated.rotationRadians.reset(); break;
    case UIAnimatedProperty::Opacity: had = node->animated.opacity.has_value(); node->animated.opacity.reset(); break;
    case UIAnimatedProperty::ColorMultiplier: had = node->animated.colorMultiplier.has_value(); node->animated.colorMultiplier.reset(); break;
    }
    if (had && node->visible && (!node->layoutState.arrangeValid || node->layoutState.effectiveVisible))
        scene.MarkDirty(handle, AnimatedPropertyDirtyFlags(property));
    return had;
}

bool SetAnimationChannelValue(UIScene& scene, UINodeHandle handle, UIAnimatedProperty property,
    UIAnimationChannel channel, const UIAnimationValue& value) {
    if (channel == UIAnimationChannel::Application)
        return SetAnimatedOverride(scene, handle, property, value);
    if (property == UIAnimatedProperty::VisualSize || !IsAnimationValueCompatible(property, value)) return false;
    UINodeRecord* node = scene.TryGet(handle);
    if (!node) return false;
    UIStyleRuntimeProperties& style = node->style.runtimeProperties;
    bool changed = false;
    switch (property) {
    case UIAnimatedProperty::Position: {
        const auto next = Finite(std::get<Engine::Vec2F>(value));
        changed = style.positionOffset != next; style.positionOffset = next; break;
    }
    case UIAnimatedProperty::Scale: {
        const auto next = Finite(std::get<Engine::Vec2F>(value), {1.0f, 1.0f});
        changed = style.scaleMultiplier != next; style.scaleMultiplier = next; break;
    }
    case UIAnimatedProperty::Rotation: {
        const float next = Finite(std::get<float>(value));
        changed = style.rotationOffsetRadians != next; style.rotationOffsetRadians = next; break;
    }
    case UIAnimatedProperty::Opacity: {
        const float next = std::clamp(Finite(std::get<float>(value), 1.0f), 0.0f, 1.0f);
        changed = style.opacityMultiplier != next; style.opacityMultiplier = next; break;
    }
    case UIAnimatedProperty::ColorMultiplier: {
        const auto next = SafeColor(std::get<Engine::Color4f>(value));
        changed = style.colorMultiplier != next; style.colorMultiplier = next; break;
    }
    case UIAnimatedProperty::VisualSize: return false;
    }
    if (!changed) return false;
    ++node->style.styleRevision;
    if (!node->visible || (node->layoutState.arrangeValid && !node->layoutState.effectiveVisible)) return true;
    scene.MarkDirty(handle, AnimatedPropertyDirtyFlags(property));
    return true;
}

bool ClearAnimationChannelValue(UIScene& scene, UINodeHandle handle, UIAnimatedProperty property,
    UIAnimationChannel channel) {
    if (channel == UIAnimationChannel::Application) return ClearAnimatedOverride(scene, handle, property);
    UIAnimationValue identity = 0.0f;
    switch (property) {
    case UIAnimatedProperty::Position: identity = Engine::Vec2F{}; break;
    case UIAnimatedProperty::Scale: identity = Engine::Vec2F{1.0f, 1.0f}; break;
    case UIAnimatedProperty::Rotation: identity = 0.0f; break;
    case UIAnimatedProperty::Opacity: identity = 1.0f; break;
    case UIAnimatedProperty::ColorMultiplier: identity = Engine::Color4f{}; break;
    case UIAnimatedProperty::VisualSize: return false;
    }
    return SetAnimationChannelValue(scene, handle, property, channel, identity);
}

void UpdateAnimatedTransforms(UIScene& scene) {
    // Opacity is intentionally Visual-only dirty, but effectiveOpacity is a
    // parent-composed runtime value. Re-evaluate it on visual updates without
    // promoting opacity animations to Transform or Layout dirty.
    if (!scene.HasDirty(UIDirtyFlags::Transform | UIDirtyFlags::Visual)) return;
    for (const UITraversalEntry& entry : scene.PainterTraversal()) {
        UINodeRecord* node = scene.TryGet(entry.node);
        if (!node || !node->layoutState.arrangeValid) continue;
        const UIResolvedAnimatedProperties resolved = ResolveAnimatedProperties(*node);
        const UINodeRecord* parent = scene.TryGet(node->parent);
        const bool root = entry.node == scene.Root();
        const Engine::Mat3F parentToScene = parent ? parent->layoutState.computedTransform.localToScene : Engine::Mat3F{};
        const Engine::Vec2F design = Finite(node->transform.position);
        const Engine::Vec2F offset{resolved.position.x - design.x, resolved.position.y - design.y};
        const Engine::Vec2F pivot{Finite(node->transform.pivot.x) * resolved.visualSize.x,
            Finite(node->transform.pivot.y) * resolved.visualSize.y};
        const Engine::Vec2F origin = root ? offset : Engine::Vec2F{
            node->layoutState.arrangedRect.x + offset.x, node->layoutState.arrangedRect.y + offset.y};
        const Engine::Mat3F local = Engine::Multiply(Engine::TranslationMatrix(origin),
            Engine::Multiply(Engine::TranslationMatrix(pivot),
                Engine::Multiply(Engine::RotationMatrix(resolved.rotationRadians),
                    Engine::Multiply(Engine::ScaleMatrix(resolved.scale),
                        Engine::TranslationMatrix({-pivot.x, -pivot.y})))));
        node->layoutState.computedTransform.localToParent = local;
        node->layoutState.computedTransform.localToScene = root ? local : Engine::Multiply(parentToScene, local);
        node->layoutState.computedTransform.inverseValid = Engine::TryInverseAffine(
            node->layoutState.computedTransform.localToScene, node->layoutState.computedTransform.sceneToLocal);
        if (!node->layoutState.computedTransform.inverseValid) node->layoutState.computedTransform.sceneToLocal = {};
        node->layoutState.sceneRect = TransformBounds(node->layoutState.computedTransform.localToScene, resolved.visualSize);
        node->layoutState.effectiveOpacity = (parent ? parent->layoutState.effectiveOpacity : 1.0f) * resolved.opacity;
    }
    scene.ClearDirty(UIDirtyFlags::Transform);
}

} // namespace Engine::UI2D
