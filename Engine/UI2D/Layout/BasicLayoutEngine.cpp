#include "UI2D/Layout/BasicLayoutEngine.h"

#include "Renderer2D/DrawTypes2D.h"
#include "Renderer2D/ITextLayoutService.h"
#include "UI2D/Core/UIScene.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <type_traits>

namespace Engine::UI2D {
namespace {

constexpr float kMaximumLayoutExtent = 1000000.0f;
constexpr std::uint32_t kMaximumLayoutDepth = 512;
constexpr float kEpsilon = 1.0e-4f;

float Finite(float value, float fallback = 0.0f) {
    return std::isfinite(value) ? value : fallback;
}

float NonNegative(float value) {
    return std::clamp(Finite(value), 0.0f, kMaximumLayoutExtent);
}

Engine::Vec2F NonNegative(Engine::Vec2F value) {
    return {NonNegative(value.x), NonNegative(value.y)};
}

Engine::RectF SafeRect(Engine::RectF value) {
    value.x = Finite(value.x);
    value.y = Finite(value.y);
    value.width = NonNegative(value.width);
    value.height = NonNegative(value.height);
    return value;
}

Engine::InsetsF SafeEdges(Engine::InsetsF value) {
    value.left = Finite(value.left);
    value.top = Finite(value.top);
    value.right = Finite(value.right);
    value.bottom = Finite(value.bottom);
    return value;
}

float Horizontal(const Engine::InsetsF& edges) { return edges.left + edges.right; }
float Vertical(const Engine::InsetsF& edges) { return edges.top + edges.bottom; }

float ClampDimension(float value, float minimum, float maximum) {
    const float safeMinimum = NonNegative(minimum);
    const float safeMaximum = std::max(safeMinimum, NonNegative(maximum));
    return std::clamp(NonNegative(value), safeMinimum, safeMaximum);
}

float ResolveMeasuredLength(const UILength& length, float natural, float available, float minimum, float maximum) {
    float value = natural;
    if (length.mode == UISizeMode::Fixed) value = length.value;
    if (length.mode == UISizeMode::Stretch) value = available;
    return ClampDimension(value, minimum, maximum);
}

float ResolveArrangedLength(const UILength& length, float desired, float available, float minimum, float maximum) {
    const float value = length.mode == UISizeMode::Stretch ? available : desired;
    return ClampDimension(value, minimum, maximum);
}

bool Near(float a, float b) { return std::abs(a - b) <= kEpsilon; }
bool Near(Engine::Vec2F a, Engine::Vec2F b) { return Near(a.x, b.x) && Near(a.y, b.y); }
bool Near(Engine::RectF a, Engine::RectF b) {
    return Near(a.x, b.x) && Near(a.y, b.y) && Near(a.width, b.width) && Near(a.height, b.height);
}
bool Near(const Engine::Mat3F& a, const Engine::Mat3F& b) {
    return Near(a.m00, b.m00) && Near(a.m01, b.m01) && Near(a.m02, b.m02) &&
        Near(a.m10, b.m10) && Near(a.m11, b.m11) && Near(a.m12, b.m12) &&
        Near(a.m20, b.m20) && Near(a.m21, b.m21) && Near(a.m22, b.m22);
}

bool SameFinalState(const UILayoutState& a, const UILayoutState& b) {
    return Near(a.desiredSize, b.desiredSize) && Near(a.arrangedRect, b.arrangedRect) &&
        Near(a.contentRect, b.contentRect) && Near(a.sceneRect, b.sceneRect) &&
        Near(a.computedTransform.localToParent, b.computedTransform.localToParent) &&
        Near(a.computedTransform.localToScene, b.computedTransform.localToScene) &&
        Near(a.computedTransform.sceneToLocal, b.computedTransform.sceneToLocal) &&
        a.computedTransform.inverseValid == b.computedTransform.inverseValid &&
        Near(a.effectiveOpacity, b.effectiveOpacity) &&
        a.effectiveVisible == b.effectiveVisible &&
        a.effectiveEnabled == b.effectiveEnabled &&
        a.measureValid == b.measureValid && a.arrangeValid == b.arrangeValid;
}

Engine::RectF TransformBounds(const Engine::Mat3F& transform, float width, float height) {
    const Engine::Vec2F points[] = {
        Engine::TransformPoint(transform, {0.0f, 0.0f}),
        Engine::TransformPoint(transform, {width, 0.0f}),
        Engine::TransformPoint(transform, {0.0f, height}),
        Engine::TransformPoint(transform, {width, height})};
    float minX = points[0].x;
    float minY = points[0].y;
    float maxX = points[0].x;
    float maxY = points[0].y;
    for (const Engine::Vec2F point : points) {
        minX = std::min(minX, point.x);
        minY = std::min(minY, point.y);
        maxX = std::max(maxX, point.x);
        maxY = std::max(maxY, point.y);
    }
    return SafeRect({minX, minY, maxX - minX, maxY - minY});
}

UIAnchor SafeAnchor(UIAnchor anchor) {
    anchor.minX = std::clamp(Finite(anchor.minX), 0.0f, 1.0f);
    anchor.minY = std::clamp(Finite(anchor.minY), 0.0f, 1.0f);
    anchor.maxX = std::clamp(Finite(anchor.maxX), 0.0f, 1.0f);
    anchor.maxY = std::clamp(Finite(anchor.maxY), 0.0f, 1.0f);
    if (anchor.minX > anchor.maxX) std::swap(anchor.minX, anchor.maxX);
    if (anchor.minY > anchor.maxY) std::swap(anchor.minY, anchor.maxY);
    return anchor;
}

float AlignedPosition(
    float regionStart,
    float regionSize,
    float marginStart,
    float marginEnd,
    float childSize,
    UIHorizontalAlignment alignment) {
    const float available = std::max(0.0f, regionSize - marginStart - marginEnd);
    if (alignment == UIHorizontalAlignment::Center) return regionStart + marginStart + (available - childSize) * 0.5f;
    if (alignment == UIHorizontalAlignment::End) return regionStart + regionSize - marginEnd - childSize;
    return regionStart + marginStart;
}

float AlignedPosition(
    float regionStart,
    float regionSize,
    float marginStart,
    float marginEnd,
    float childSize,
    UIVerticalAlignment alignment) {
    const float available = std::max(0.0f, regionSize - marginStart - marginEnd);
    if (alignment == UIVerticalAlignment::Center) return regionStart + marginStart + (available - childSize) * 0.5f;
    if (alignment == UIVerticalAlignment::End) return regionStart + regionSize - marginEnd - childSize;
    return regionStart + marginStart;
}

const char* ModeName(UILayoutMode mode) {
    switch (mode) {
    case UILayoutMode::Absolute: return "Absolute";
    case UILayoutMode::Anchor: return "Anchor";
    case UILayoutMode::HorizontalStack: return "HorizontalStack";
    case UILayoutMode::VerticalStack: return "VerticalStack";
    case UILayoutMode::Overlay: return "Overlay";
    }
    return "Unknown";
}

} // namespace

BasicLayoutEngine::BasicLayoutEngine(
    const Engine::ITextureInfoProvider2D* textures,
    const Engine::ITextLayoutService* textLayouts,
    Engine::Vec2F unavailableImagePlaceholder)
    : m_Textures(textures),
      m_TextLayouts(textLayouts),
      m_UnavailableImagePlaceholder(NonNegative(unavailableImagePlaceholder)) {}

void BasicLayoutEngine::SetError(UILayoutError error) {
    if (m_Error == UILayoutError::None) m_Error = error;
}

Engine::Vec2F BasicLayoutEngine::MeasureIntrinsic(const UINodeRecord& node) const {
    Engine::Vec2F size = NonNegative(node.transform.size);
    std::visit([&](const auto& visual) {
        using T = std::decay_t<decltype(visual)>;
        if constexpr (std::is_same_v<T, UIImageVisual>) {
            if ((size.x <= 0.0f || size.y <= 0.0f) && m_Textures != nullptr) {
                const Engine::TextureInfo info = m_Textures->GetInfo(visual.texture);
                if (size.x <= 0.0f && info.width > 0) size.x = static_cast<float>(info.width);
                if (size.y <= 0.0f && info.height > 0) size.y = static_cast<float>(info.height);
            }
            if (size.x <= 0.0f) size.x = m_UnavailableImagePlaceholder.x;
            if (size.y <= 0.0f) size.y = m_UnavailableImagePlaceholder.y;
        } else if constexpr (std::is_same_v<T, UITextVisual>) {
            if (m_TextLayouts != nullptr) {
                if (const Engine::TextLayoutBitmap* bitmap = m_TextLayouts->GetBitmap(visual.layout)) {
                    if (size.x <= 0.0f) size.x = static_cast<float>(bitmap->width);
                    if (size.y <= 0.0f) size.y = static_cast<float>(bitmap->height);
                }
            }
        } else if constexpr (std::is_same_v<T, UINineSliceVisual>) {
            if (size.x <= 0.0f) size.x = NonNegative(visual.borders.left + visual.borders.right);
            if (size.y <= 0.0f) size.y = NonNegative(visual.borders.top + visual.borders.bottom);
            if ((size.x <= 0.0f || size.y <= 0.0f) && m_Textures != nullptr) {
                const Engine::TextureInfo info = m_Textures->GetInfo(visual.texture);
                if (size.x <= 0.0f && info.width > 0) size.x = static_cast<float>(info.width);
                if (size.y <= 0.0f && info.height > 0) size.y = static_cast<float>(info.height);
            }
        } else if constexpr (std::is_same_v<T, UIShapeVisual>) {
            if (size.x <= 0.0f) size.x = std::abs(Finite(visual.lineTo.x));
            if (size.y <= 0.0f) size.y = std::abs(Finite(visual.lineTo.y));
        }
    }, node.visual);
    return NonNegative(size);
}

Engine::Vec2F BasicLayoutEngine::MeasureNode(
    UIScene& scene,
    UINodeHandle handle,
    Engine::Vec2F availableSize,
    const UILayoutContext& context,
    std::uint32_t depth) {
    (void)context;
    if (depth > kMaximumLayoutDepth) {
        SetError(UILayoutError::MaximumDepthExceeded);
        return {};
    }
    m_DebugStats.maxDepth = std::max(m_DebugStats.maxDepth, depth);
    if (!handle.IsValid() || handle.index >= m_VisitStates.size()) {
        SetError(UILayoutError::InvalidRoot);
        return {};
    }
    if (m_VisitStates[handle.index] == 1) {
        SetError(UILayoutError::CycleDetected);
        return {};
    }
    UINodeRecord* node = scene.TryGet(handle);
    if (!node) {
        SetError(UILayoutError::InvalidRoot);
        return {};
    }
    m_VisitStates[handle.index] = 1;
    ++m_DebugStats.measuredNodes;
    availableSize = NonNegative(availableSize);

    const Engine::Vec2F previousDesired = node->layoutState.desiredSize;
    const bool previousValid = node->layoutState.measureValid;
    if (!node->visible) {
        node->layoutState.desiredSize = {};
        node->layoutState.measureValid = true;
        m_LayoutChanged |= previousValid == false || !Near(previousDesired, Engine::Vec2F{});
        m_VisitStates[handle.index] = 2;
        return {};
    }
    ++m_DebugStats.visibleNodes;

    const Engine::InsetsF padding = SafeEdges(node->layout.padding);
    const Engine::Vec2F innerAvailable{
        NonNegative(availableSize.x - Horizontal(padding)),
        NonNegative(availableSize.y - Vertical(padding))};
    Engine::Vec2F childrenNatural{};
    std::size_t visibleChildren = 0;
    float absoluteMinX = 0.0f;
    float absoluteMinY = 0.0f;
    float absoluteMaxX = 0.0f;
    float absoluteMaxY = 0.0f;

    for (UINodeHandle childHandle : scene.Children(handle)) {
        UINodeRecord* child = scene.TryGet(childHandle);
        if (!child) continue;
        const Engine::InsetsF margin = SafeEdges(child->layout.margin);
        Engine::Vec2F childAvailable{
            NonNegative(innerAvailable.x - Horizontal(margin)),
            NonNegative(innerAvailable.y - Vertical(margin))};
        if (node->layout.mode == UILayoutMode::HorizontalStack && child->layout.sizeRule.width.mode == UISizeMode::Stretch) {
            childAvailable.x = 0.0f;
        }
        if (node->layout.mode == UILayoutMode::VerticalStack && child->layout.sizeRule.height.mode == UISizeMode::Stretch) {
            childAvailable.y = 0.0f;
        }
        const Engine::Vec2F desired = MeasureNode(scene, childHandle, childAvailable, context, depth + 1);
        if (!child->visible) continue;
        ++visibleChildren;
        const float outerWidth = NonNegative(desired.x + Horizontal(margin));
        const float outerHeight = NonNegative(desired.y + Vertical(margin));

        switch (node->layout.mode) {
        case UILayoutMode::Absolute: {
            const float left = Finite(child->transform.position.x) + margin.left;
            const float top = Finite(child->transform.position.y) + margin.top;
            absoluteMinX = std::min(absoluteMinX, left);
            absoluteMinY = std::min(absoluteMinY, top);
            absoluteMaxX = std::max(absoluteMaxX, left + desired.x + margin.right);
            absoluteMaxY = std::max(absoluteMaxY, top + desired.y + margin.bottom);
            break;
        }
        case UILayoutMode::HorizontalStack:
            childrenNatural.x += outerWidth;
            childrenNatural.y = std::max(childrenNatural.y, outerHeight);
            break;
        case UILayoutMode::VerticalStack:
            childrenNatural.x = std::max(childrenNatural.x, outerWidth);
            childrenNatural.y += outerHeight;
            break;
        case UILayoutMode::Anchor:
        case UILayoutMode::Overlay:
            childrenNatural.x = std::max(childrenNatural.x, outerWidth);
            childrenNatural.y = std::max(childrenNatural.y, outerHeight);
            break;
        }
    }

    if (node->layout.mode == UILayoutMode::Absolute) {
        childrenNatural = {absoluteMaxX - absoluteMinX, absoluteMaxY - absoluteMinY};
    }
    if (visibleChildren > 1) {
        const float spacing = NonNegative(node->layout.spacing) * static_cast<float>(visibleChildren - 1);
        if (node->layout.mode == UILayoutMode::HorizontalStack) childrenNatural.x += spacing;
        if (node->layout.mode == UILayoutMode::VerticalStack) childrenNatural.y += spacing;
    }

    const Engine::Vec2F intrinsic = MeasureIntrinsic(*node);
    Engine::Vec2F natural{
        std::max(intrinsic.x, childrenNatural.x + Horizontal(padding)),
        std::max(intrinsic.y, childrenNatural.y + Vertical(padding))};
    const UISizeRule& rule = node->layout.sizeRule;
    Engine::Vec2F desired{
        ResolveMeasuredLength(rule.width, natural.x, availableSize.x, rule.minSize.x, rule.maxSize.x),
        ResolveMeasuredLength(rule.height, natural.y, availableSize.y, rule.minSize.y, rule.maxSize.y)};
    desired = NonNegative(desired);
    node->layoutState.desiredSize = desired;
    node->layoutState.measureValid = true;
    m_LayoutChanged |= !previousValid || !Near(previousDesired, desired);
    m_VisitStates[handle.index] = 2;
    return desired;
}

void BasicLayoutEngine::ArrangeNode(
    UIScene& scene,
    UINodeHandle handle,
    Engine::RectF finalRect,
    const UILayoutContext& context,
    std::uint32_t depth) {
    if (m_Error != UILayoutError::None) return;
    if (depth > kMaximumLayoutDepth) {
        SetError(UILayoutError::MaximumDepthExceeded);
        return;
    }
    if (!handle.IsValid() || handle.index >= m_VisitStates.size()) {
        SetError(UILayoutError::InvalidRoot);
        return;
    }
    if (m_VisitStates[handle.index] == 1) {
        SetError(UILayoutError::CycleDetected);
        return;
    }
    UINodeRecord* node = scene.TryGet(handle);
    if (!node) {
        SetError(UILayoutError::InvalidRoot);
        return;
    }
    m_VisitStates[handle.index] = 1;
    ++m_DebugStats.arrangedNodes;

    finalRect = SafeRect(finalRect);
    const UINodeRecord* visibilityParent = scene.TryGet(node->parent);
    const bool parentVisible = visibilityParent == nullptr || visibilityParent->layoutState.effectiveVisible;
    const bool effectiveVisible = parentVisible && node->visible;
    const bool parentEnabled = visibilityParent == nullptr || visibilityParent->layoutState.effectiveEnabled;
    const bool effectiveEnabled = parentEnabled && node->enabled;
    if (!effectiveVisible) {
        finalRect.width = 0.0f;
        finalRect.height = 0.0f;
    }
    const UILayoutState previous = node->layoutState;
    UILayoutState next = previous;
    next.effectiveVisible = effectiveVisible;
    next.effectiveEnabled = effectiveEnabled;
    next.arrangedRect = finalRect;
    const Engine::InsetsF padding = SafeEdges(node->layout.padding);
    next.contentRect = {
        padding.left,
        padding.top,
        NonNegative(finalRect.width - Horizontal(padding)),
        NonNegative(finalRect.height - Vertical(padding))};

    if (handle == context.layoutRoot || handle == scene.Root()) {
        next.computedTransform.localToParent = {};
        next.computedTransform.localToScene = {};
        next.effectiveOpacity = std::clamp(Finite(node->opacity, 1.0f), 0.0f, 1.0f);
    } else {
        const UINodeRecord* parent = scene.TryGet(node->parent);
        const Engine::Mat3F parentToScene = parent ? parent->layoutState.computedTransform.localToScene : Engine::Mat3F{};
        const Engine::Vec2F pivot{
            Finite(node->transform.pivot.x) * finalRect.width,
            Finite(node->transform.pivot.y) * finalRect.height};
        Engine::Vec2F scale{Finite(node->transform.scale.x, 1.0f), Finite(node->transform.scale.y, 1.0f)};
        const float rotation = Finite(node->transform.rotationRadians);
        const Engine::Mat3F local = Engine::Multiply(
            Engine::TranslationMatrix({finalRect.x, finalRect.y}),
            Engine::Multiply(
                Engine::TranslationMatrix(pivot),
                Engine::Multiply(
                    Engine::RotationMatrix(rotation),
                    Engine::Multiply(Engine::ScaleMatrix(scale), Engine::TranslationMatrix({-pivot.x, -pivot.y})))));
        next.computedTransform.localToParent = local;
        next.computedTransform.localToScene = Engine::Multiply(parentToScene, local);
        const float parentOpacity = parent ? parent->layoutState.effectiveOpacity : 1.0f;
        next.effectiveOpacity = parentOpacity * std::clamp(Finite(node->opacity, 1.0f), 0.0f, 1.0f);
    }
    next.computedTransform.inverseValid = Engine::TryInverseAffine(
        next.computedTransform.localToScene, next.computedTransform.sceneToLocal);
    if (!next.computedTransform.inverseValid) next.computedTransform.sceneToLocal = {};
    next.sceneRect = TransformBounds(next.computedTransform.localToScene, finalRect.width, finalRect.height);
    next.arrangeValid = true;
    node->layoutState = next;
    m_LayoutChanged |= !SameFinalState(previous, next);

    ArrangeChildren(scene, *node, context, depth);
    m_VisitStates[handle.index] = 2;
}

void BasicLayoutEngine::ArrangeChildren(
    UIScene& scene,
    UINodeRecord& node,
    const UILayoutContext& context,
    std::uint32_t depth) {
    const Engine::RectF content = node.layoutState.contentRect;
    const float spacing = NonNegative(node.layout.spacing);

    if (node.layout.mode == UILayoutMode::HorizontalStack || node.layout.mode == UILayoutMode::VerticalStack) {
        std::size_t visibleCount = 0;
        std::size_t stretchCount = 0;
        float fixedOuter = 0.0f;
        for (UINodeHandle childHandle : scene.Children(node.handle)) {
            const UINodeRecord* child = scene.TryGet(childHandle);
            if (!child || !child->visible) continue;
            ++visibleCount;
            const Engine::InsetsF margin = SafeEdges(child->layout.margin);
            const bool stretch = node.layout.mode == UILayoutMode::HorizontalStack
                ? child->layout.sizeRule.width.mode == UISizeMode::Stretch
                : child->layout.sizeRule.height.mode == UISizeMode::Stretch;
            if (stretch) {
                ++stretchCount;
                fixedOuter += node.layout.mode == UILayoutMode::HorizontalStack ? Horizontal(margin) : Vertical(margin);
            } else {
                fixedOuter += node.layout.mode == UILayoutMode::HorizontalStack
                    ? child->layoutState.desiredSize.x + Horizontal(margin)
                    : child->layoutState.desiredSize.y + Vertical(margin);
            }
        }
        const float mainAvailable = node.layout.mode == UILayoutMode::HorizontalStack ? content.width : content.height;
        const float totalSpacing = visibleCount > 1 ? spacing * static_cast<float>(visibleCount - 1) : 0.0f;
        const float share = stretchCount > 0
            ? NonNegative(mainAvailable - fixedOuter - totalSpacing) / static_cast<float>(stretchCount)
            : 0.0f;
        float cursor = node.layout.mode == UILayoutMode::HorizontalStack ? content.x : content.y;
        std::size_t arrangedVisible = 0;
        for (UINodeHandle childHandle : scene.Children(node.handle)) {
            UINodeRecord* child = scene.TryGet(childHandle);
            if (!child || !child->visible) {
                if (child) ArrangeNode(scene, childHandle, {cursor, content.y, 0.0f, 0.0f}, context, depth + 1);
                continue;
            }
            const Engine::InsetsF margin = SafeEdges(child->layout.margin);
            const UISizeRule& rule = child->layout.sizeRule;
            Engine::RectF rect{};
            if (node.layout.mode == UILayoutMode::HorizontalStack) {
                const float width = ResolveArrangedLength(rule.width, child->layoutState.desiredSize.x,
                    rule.width.mode == UISizeMode::Stretch ? share : content.width - Horizontal(margin),
                    rule.minSize.x, rule.maxSize.x);
                const float height = ResolveArrangedLength(rule.height, child->layoutState.desiredSize.y,
                    NonNegative(content.height - Vertical(margin)), rule.minSize.y, rule.maxSize.y);
                rect = {
                    cursor + margin.left + Finite(child->transform.position.x),
                    AlignedPosition(content.y, content.height, margin.top, margin.bottom, height,
                        child->layout.verticalAlignment) + Finite(child->transform.position.y),
                    width,
                    height};
                cursor += margin.left + width + margin.right;
            } else {
                const float width = ResolveArrangedLength(rule.width, child->layoutState.desiredSize.x,
                    NonNegative(content.width - Horizontal(margin)), rule.minSize.x, rule.maxSize.x);
                const float height = ResolveArrangedLength(rule.height, child->layoutState.desiredSize.y,
                    rule.height.mode == UISizeMode::Stretch ? share : content.height - Vertical(margin),
                    rule.minSize.y, rule.maxSize.y);
                rect = {
                    AlignedPosition(content.x, content.width, margin.left, margin.right, width,
                        child->layout.horizontalAlignment) + Finite(child->transform.position.x),
                    cursor + margin.top + Finite(child->transform.position.y),
                    width,
                    height};
                cursor += margin.top + height + margin.bottom;
            }
            ++arrangedVisible;
            if (arrangedVisible < visibleCount) cursor += spacing;
            ArrangeNode(scene, childHandle, rect, context, depth + 1);
        }
        return;
    }

    for (UINodeHandle childHandle : scene.Children(node.handle)) {
        UINodeRecord* child = scene.TryGet(childHandle);
        if (!child) continue;
        if (!child->visible) {
            ArrangeNode(scene, childHandle, {content.x, content.y, 0.0f, 0.0f}, context, depth + 1);
            continue;
        }
        const Engine::InsetsF margin = SafeEdges(child->layout.margin);
        const UISizeRule& rule = child->layout.sizeRule;
        const float availableWidth = NonNegative(content.width - Horizontal(margin));
        const float availableHeight = NonNegative(content.height - Vertical(margin));
        float width = ResolveArrangedLength(rule.width, child->layoutState.desiredSize.x,
            availableWidth, rule.minSize.x, rule.maxSize.x);
        float height = ResolveArrangedLength(rule.height, child->layoutState.desiredSize.y,
            availableHeight, rule.minSize.y, rule.maxSize.y);
        Engine::RectF rect{};

        if (node.layout.mode == UILayoutMode::Absolute) {
            rect = {
                content.x + margin.left + Finite(child->transform.position.x),
                content.y + margin.top + Finite(child->transform.position.y),
                width,
                height};
        } else if (node.layout.mode == UILayoutMode::Overlay) {
            rect = {
                AlignedPosition(content.x, content.width, margin.left, margin.right, width,
                    child->layout.horizontalAlignment) + Finite(child->transform.position.x),
                AlignedPosition(content.y, content.height, margin.top, margin.bottom, height,
                    child->layout.verticalAlignment) + Finite(child->transform.position.y),
                width,
                height};
        } else {
            const UIAnchor anchor = SafeAnchor(child->layout.anchor);
            const float anchorLeft = content.x + content.width * anchor.minX;
            const float anchorTop = content.y + content.height * anchor.minY;
            const float anchorRight = content.x + content.width * anchor.maxX;
            const float anchorBottom = content.y + content.height * anchor.maxY;
            const float regionLeft = anchorLeft + Finite(child->layout.offsetMin.x) + margin.left;
            const float regionTop = anchorTop + Finite(child->layout.offsetMin.y) + margin.top;
            const float regionRight = anchorRight + Finite(child->layout.offsetMax.x) - margin.right;
            const float regionBottom = anchorBottom + Finite(child->layout.offsetMax.y) - margin.bottom;
            const float regionWidth = NonNegative(regionRight - regionLeft);
            const float regionHeight = NonNegative(regionBottom - regionTop);
            if (anchor.minX != anchor.maxX && rule.width.mode == UISizeMode::Stretch) {
                width = ClampDimension(regionWidth, rule.minSize.x, rule.maxSize.x);
            }
            if (anchor.minY != anchor.maxY && rule.height.mode == UISizeMode::Stretch) {
                height = ClampDimension(regionHeight, rule.minSize.y, rule.maxSize.y);
            }
            rect = {
                AlignedPosition(regionLeft, regionWidth, 0.0f, 0.0f, width,
                    child->layout.horizontalAlignment) + Finite(child->transform.position.x),
                AlignedPosition(regionTop, regionHeight, 0.0f, 0.0f, height,
                    child->layout.verticalAlignment) + Finite(child->transform.position.y),
                width,
                height};
            if (anchor.minX == anchor.maxX) rect.x = anchorLeft + Finite(child->layout.offsetMin.x) + margin.left + Finite(child->transform.position.x);
            if (anchor.minY == anchor.maxY) rect.y = anchorTop + Finite(child->layout.offsetMin.y) + margin.top + Finite(child->transform.position.y);
        }
        ArrangeNode(scene, childHandle, rect, context, depth + 1);
    }
}

UILayoutResult BasicLayoutEngine::UpdateLayout(UIScene& scene, const UILayoutContext& context) {
    UILayoutResult result;
    if (!scene.HasDirty(UIDirtyFlags::Layout)) return result;

    const UINodeHandle rootHandle = context.layoutRoot.IsValid() ? context.layoutRoot : scene.Root();
    UINodeRecord* root = scene.TryGet(rootHandle);
    if (!root) {
        result.error = UILayoutError::InvalidRoot;
        return result;
    }

    m_DebugStats = {};
    m_DebugStats.totalNodes = static_cast<std::uint32_t>(scene.NodeCount());
    m_Error = UILayoutError::None;
    m_LayoutChanged = false;
    std::size_t maximumIndex = rootHandle.index;
    for (UINodeHandle handle : scene.LiveNodes()) maximumIndex = std::max(maximumIndex, static_cast<std::size_t>(handle.index));
    m_VisitStates.assign(maximumIndex + 1, 0);

    const Engine::Vec2F logicalSize = NonNegative(context.logicalWindowSize);
    (void)MeasureNode(scene, rootHandle, logicalSize, context, 0);
    if (m_Error == UILayoutError::None) {
        std::fill(m_VisitStates.begin(), m_VisitStates.end(), 0);
        ArrangeNode(scene, rootHandle, {0.0f, 0.0f, logicalSize.x, logicalSize.y}, context, 0);
    }

    result.measuredNodeCount = m_DebugStats.measuredNodes;
    result.arrangedNodeCount = m_DebugStats.arrangedNodes;
    result.error = m_Error;
    if (m_Error != UILayoutError::None) return result;

    scene.ClearDirty(UIDirtyFlags::Layout);
    result.layoutChanged = m_LayoutChanged;
    if (m_LayoutChanged) {
        scene.InvalidateLayoutRevision();
        scene.MarkAllDirty(UIDirtyFlags::Transform | UIDirtyFlags::Visual | UIDirtyFlags::HitTest);
        result.visualInvalidated = true;
        result.hitTestInvalidated = true;
    }
    return result;
}

std::string BasicLayoutEngine::DumpScene(const UIScene& scene) const {
    std::ostringstream output;
    output << std::fixed << std::setprecision(2);
    const auto dumpNode = [&](const auto& self, UINodeHandle handle, std::uint32_t depth) -> void {
        if (depth > kMaximumLayoutDepth) return;
        const UINodeRecord* node = scene.TryGet(handle);
        if (!node) return;
        output << std::string(depth * 2, ' ') << (node->debugName.empty() ? "<unnamed>" : node->debugName)
               << " mode=" << ModeName(node->layout.mode)
               << " rect=(" << node->layoutState.arrangedRect.x << ',' << node->layoutState.arrangedRect.y
               << ',' << node->layoutState.arrangedRect.width << ',' << node->layoutState.arrangedRect.height << ")\n";
        for (UINodeHandle child : scene.Children(handle)) self(self, child, depth + 1);
    };
    dumpNode(dumpNode, scene.Root(), 0);
    return output.str();
}

} // namespace Engine::UI2D
