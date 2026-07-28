#include "UI2D/Rendering/BasicUIRenderBuilder.h"

#include "Renderer2D/DrawListValidator.h"
#include "Renderer2D/ITextLayoutService.h"
#include "UI2D/Animation/UIAnimatedProperties.h"
#include "UI2D/Core/UIScene.h"
#include "UI2D/Rendering/UIRenderGeometry.h"
#include "UI2D/Theme/ResolvedUITheme.h"

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <vector>

namespace Engine::UI2D {
namespace {

constexpr float kOpacityEpsilon = 1.0e-5f;

bool Finite(float value) noexcept { return std::isfinite(value); }
bool Finite(Engine::Vec2F value) noexcept { return Finite(value.x) && Finite(value.y); }
bool Finite(Engine::RectF value) noexcept {
    return Finite(value.x) && Finite(value.y) && Finite(value.width) && Finite(value.height);
}
bool Finite(Engine::Color4f value) noexcept {
    return Finite(value.r) && Finite(value.g) && Finite(value.b) && Finite(value.a);
}
bool Finite(const Engine::Mat3F& value) noexcept {
    return Finite(value.m00) && Finite(value.m01) && Finite(value.m02) &&
        Finite(value.m10) && Finite(value.m11) && Finite(value.m12) &&
        Finite(value.m20) && Finite(value.m21) && Finite(value.m22);
}

Engine::RectF LocalRect(const UINodeRecord& node) noexcept {
    const Engine::Vec2F size = ResolveAnimatedProperties(node).visualSize;
    return {0.0f, 0.0f, size.x, size.y};
}

bool PositiveRect(Engine::RectF value) noexcept {
    return Finite(value) && value.width > 0.0f && value.height > 0.0f;
}

Engine::Color4f ResolveColor(Engine::Color4f source, float opacity, Engine::AlphaMode mode,
    Engine::Color4f multiplier = {}) noexcept {
    source.r = std::clamp(source.r * multiplier.r, 0.0f, 1.0f);
    source.g = std::clamp(source.g * multiplier.g, 0.0f, 1.0f);
    source.b = std::clamp(source.b * multiplier.b, 0.0f, 1.0f);
    const float alpha = std::clamp(source.a * multiplier.a, 0.0f, 1.0f) * std::clamp(opacity, 0.0f, 1.0f);
    source.a = alpha;
    if (mode == Engine::AlphaMode::Premultiplied) {
        source.r *= alpha;
        source.g *= alpha;
        source.b *= alpha;
    }
    return source;
}

Engine::InsetsF SelectBorders(Engine::InsetsF preferred, Engine::InsetsF fallback) noexcept {
    if (preferred.left > 0.0f || preferred.top > 0.0f ||
        preferred.right > 0.0f || preferred.bottom > 0.0f) return preferred;
    return fallback;
}

Engine::InsetsF NormalizeDestinationBorders(Engine::InsetsF borders, Engine::RectF rect) noexcept {
    borders.left = std::max(0.0f, Finite(borders.left) ? borders.left : 0.0f);
    borders.right = std::max(0.0f, Finite(borders.right) ? borders.right : 0.0f);
    borders.top = std::max(0.0f, Finite(borders.top) ? borders.top : 0.0f);
    borders.bottom = std::max(0.0f, Finite(borders.bottom) ? borders.bottom : 0.0f);
    const float horizontal = borders.left + borders.right;
    if (horizontal > rect.width && horizontal > 0.0f) {
        const float scale = rect.width / horizontal;
        borders.left *= scale;
        borders.right *= scale;
    }
    const float vertical = borders.top + borders.bottom;
    if (vertical > rect.height && vertical > 0.0f) {
        const float scale = rect.height / vertical;
        borders.top *= scale;
        borders.bottom *= scale;
    }
    return borders;
}

} // namespace

BasicUIRenderBuilder::BasicUIRenderBuilder(
    const Engine::ITextureInfoProvider2D* textures,
    const Engine::ITextLayoutService* textLayouts)
    : m_Textures(textures), m_TextLayouts(textLayouts) {}

std::uint64_t BasicUIRenderBuilder::ResourceRevision() const {
    const std::uint64_t textures = m_Textures ? m_Textures->Revision() : 0;
    const std::uint64_t text = m_TextLayouts ? m_TextLayouts->Revision() : 0;
    return textures ^ (text + 0x9e3779b97f4a7c15ull + (textures << 6u) + (textures >> 2u));
}

UIRenderResult BasicUIRenderBuilder::Build(
    const UIScene& scene,
    const ResolvedUITheme&,
    Engine::DrawList2D& output,
    const UIRenderContext& context) {
    UIRenderResult result;
    output.Clear();
    const UINodeHandle root = context.root.IsValid() ? context.root : scene.Root();
    if (!scene.TryGet(root) || scene.HasDirty(UIDirtyFlags::Layout)) {
        result.success = false;
        return result;
    }

    const std::span<const UITraversalEntry> traversal = scene.PainterTraversal();
    std::size_t begin = traversal.size();
    std::size_t end = traversal.size();
    std::uint32_t rootDepth = 0;
    for (std::size_t i = 0; i < traversal.size(); ++i) {
        if (traversal[i].node == root) {
            begin = i;
            rootDepth = traversal[i].depth;
            end = i + 1;
            while (end < traversal.size() && traversal[end].depth > rootDepth) ++end;
            break;
        }
    }
    if (begin == traversal.size()) {
        result.success = false;
        return result;
    }

    std::vector<std::uint32_t> activeClipDepths;
    activeClipDepths.reserve(8);
    output.Reserve(end - begin + 8);
    for (std::size_t i = begin; i < end; ++i) {
        const UITraversalEntry& entry = traversal[i];
        while (!activeClipDepths.empty() && activeClipDepths.back() >= entry.depth) {
            output.PopClip();
            activeClipDepths.pop_back();
            ++result.clipPopCount;
            ++result.emittedCommandCount;
        }
        ++result.visitedNodeCount;
        const UINodeRecord* node = scene.TryGet(entry.node);
        if (!node || !node->visible || !node->layoutState.effectiveVisible ||
            !node->layoutState.arrangeValid) continue;
        if (!Finite(node->layoutState.computedTransform.localToScene)) {
            ++result.invalidTransformCount;
            result.success = false;
            continue;
        }

        if (node->layoutState.effectiveOpacity > kOpacityEpsilon &&
            !std::holds_alternative<std::monostate>(node->visual)) {
            if (EmitNode(*node, context, output, result)) ++result.emittedNodeCount;
        }

        const bool hasChildrenInRange = i + 1 < end && traversal[i + 1].depth > entry.depth;
        if (node->clipChildren && hasChildrenInRange) {
            const Engine::RectF clip = ResolveSceneClipRect(*node);
            if (PositiveRect(clip)) {
                output.PushClipRect(clip);
                activeClipDepths.push_back(entry.depth);
                ++result.clipPushCount;
                ++result.emittedCommandCount;
            }
        }
    }
    while (!activeClipDepths.empty()) {
        output.PopClip();
        activeClipDepths.pop_back();
        ++result.clipPopCount;
        ++result.emittedCommandCount;
    }

    const Engine::DrawListValidationResult validation = Engine::DrawListValidator{}.Validate(output);
    if (!validation.valid || result.clipPushCount != result.clipPopCount) result.success = false;
    result.drawListChanged = true;
    return result;
}

bool BasicUIRenderBuilder::EmitNode(
    const UINodeRecord& node,
    const UIRenderContext& context,
    Engine::DrawList2D& output,
    UIRenderResult& result) const {
    const std::size_t before = output.Commands().size();
    bool emitted = std::visit([&](const auto& visual) -> bool {
        using T = std::decay_t<decltype(visual)>;
        if constexpr (std::is_same_v<T, UIPanelVisual>) return EmitPanel(node, visual, output, result);
        if constexpr (std::is_same_v<T, UIImageVisual>) return EmitImage(node, visual, output, result);
        if constexpr (std::is_same_v<T, UITextVisual>) return EmitText(node, visual, output, result);
        if constexpr (std::is_same_v<T, UINineSliceVisual>) return EmitNineSlice(node, visual, output, result);
        if constexpr (std::is_same_v<T, UIShapeVisual>) return EmitShape(node, visual, output, result);
        return false;
    }, node.visual);

    if (context.debugDrawBounds && PositiveRect(LocalRect(node))) {
        const Engine::RectF rect = LocalRect(node);
        const Engine::Color4f color = ResolveColor({0, 1, 0, 0.7f}, 1.0f, Engine::AlphaMode::Premultiplied);
        const Engine::Vec2F corners[] = {{0,0}, {rect.width,0}, {rect.width,rect.height}, {0,rect.height}};
        for (int i = 0; i < 4; ++i) {
            Engine::LineCommand command;
            command.from = corners[i]; command.to = corners[(i + 1) % 4]; command.thickness = 1.0f;
            command.color = color; command.transform = node.layoutState.computedTransform.localToScene;
            output.AddLine(command);
        }
        emitted = true;
    }
    result.emittedCommandCount += static_cast<std::uint32_t>(output.Commands().size() - before);
    return emitted;
}

bool BasicUIRenderBuilder::EmitPanel(
    const UINodeRecord& node, const UIPanelVisual& visual,
    Engine::DrawList2D& output, UIRenderResult& result) const {
    const Engine::RectF rect = LocalRect(node);
    if (!PositiveRect(rect) || !Finite(visual.color) || !Finite(visual.cornerRadius)) {
        ++result.invalidVisualCount; return false;
    }
    const Engine::Color4f color = ResolveColor(
        visual.color, node.layoutState.effectiveOpacity, Engine::AlphaMode::Premultiplied,
        ResolveAnimatedProperties(node).colorMultiplier);
    const float radius = std::clamp(visual.cornerRadius, 0.0f, std::min(rect.width, rect.height) * 0.5f);
    if (visual.useRoundedRect || radius > 0.0f) {
        Engine::RoundedRectCommand command;
        command.rect = rect; command.radius = radius; command.color = color;
        command.transform = node.layoutState.computedTransform.localToScene;
        output.AddRoundedRect(command);
    } else {
        Engine::SolidRectCommand command;
        command.rect = rect; command.color = color;
        command.transform = node.layoutState.computedTransform.localToScene;
        output.AddSolidRect(command);
    }
    return true;
}

bool BasicUIRenderBuilder::EmitTexturePlaceholder(
    const UINodeRecord& node, Engine::TextureState state,
    Engine::DrawList2D& output, UIRenderResult& result) const {
    if (state == Engine::TextureState::Loading) ++result.loadingTextureCount;
    else if (state == Engine::TextureState::Failed) ++result.failedTextureCount;
    else ++result.missingTextureCount;
    const Engine::Color4f source = state == Engine::TextureState::Loading ? Engine::Color4f{0.35f,0.35f,0.38f,1}
        : state == Engine::TextureState::Failed ? Engine::Color4f{0.75f,0.08f,0.08f,1}
        : Engine::Color4f{0.75f,0.0f,0.75f,1};
    Engine::SolidRectCommand command;
    command.rect = LocalRect(node);
    if (!PositiveRect(command.rect)) return false;
    command.color = ResolveColor(source, node.layoutState.effectiveOpacity, Engine::AlphaMode::Premultiplied,
        ResolveAnimatedProperties(node).colorMultiplier);
    command.transform = node.layoutState.computedTransform.localToScene;
    output.AddSolidRect(command);
    return true;
}

bool BasicUIRenderBuilder::EmitImage(
    const UINodeRecord& node, const UIImageVisual& visual,
    Engine::DrawList2D& output, UIRenderResult& result) const {
    const Engine::RectF bounds = LocalRect(node);
    if (!PositiveRect(bounds) || !Finite(visual.uv) || !Finite(visual.tint)) {
        ++result.invalidVisualCount; return false;
    }
    Engine::TextureInfo info;
    info.state = Engine::TextureState::Missing;
    if (m_Textures) info = m_Textures->GetInfo(visual.texture);
    if (info.state != Engine::TextureState::Ready || info.width <= 0 || info.height <= 0) {
        return EmitTexturePlaceholder(node, info.state, output, result);
    }

    Engine::RectF dst = bounds;
    Engine::RectF uv = visual.uv;
    const float sourceWidth = static_cast<float>(info.width) * std::abs(uv.width);
    const float sourceHeight = static_cast<float>(info.height) * std::abs(uv.height);
    if (sourceWidth > 0.0f && sourceHeight > 0.0f) {
        const float sourceAspect = sourceWidth / sourceHeight;
        const float targetAspect = bounds.width / bounds.height;
        if (visual.preserveAspectRatio && visual.fit == UIImageFit::Contain) {
            if (sourceAspect > targetAspect) {
                dst.height = bounds.width / sourceAspect; dst.y = (bounds.height - dst.height) * 0.5f;
            } else {
                dst.width = bounds.height * sourceAspect; dst.x = (bounds.width - dst.width) * 0.5f;
            }
        } else if (visual.preserveAspectRatio && visual.fit == UIImageFit::Cover) {
            if (sourceAspect > targetAspect) {
                const float fraction = targetAspect / sourceAspect;
                const float old = uv.width; uv.width *= fraction; uv.x += (old - uv.width) * 0.5f;
            } else {
                const float fraction = sourceAspect / targetAspect;
                const float old = uv.height; uv.height *= fraction; uv.y += (old - uv.height) * 0.5f;
            }
        } else if (visual.fit == UIImageFit::None) {
            dst.width = std::min(bounds.width, sourceWidth); dst.height = std::min(bounds.height, sourceHeight);
            dst.x = (bounds.width - dst.width) * 0.5f; dst.y = (bounds.height - dst.height) * 0.5f;
            const float widthFraction = dst.width / sourceWidth;
            const float heightFraction = dst.height / sourceHeight;
            const float oldWidth = uv.width; const float oldHeight = uv.height;
            uv.width *= widthFraction; uv.height *= heightFraction;
            uv.x += (oldWidth - uv.width) * 0.5f; uv.y += (oldHeight - uv.height) * 0.5f;
        }
    }
    Engine::SpriteCommand command;
    command.dst = dst; command.uv = uv; command.texture = visual.texture;
    command.alphaMode = info.alphaMode;
    command.color = ResolveColor(visual.tint, node.layoutState.effectiveOpacity, info.alphaMode,
        ResolveAnimatedProperties(node).colorMultiplier);
    command.transform = node.layoutState.computedTransform.localToScene;
    output.AddSprite(command);
    return true;
}

bool BasicUIRenderBuilder::EmitText(
    const UINodeRecord& node, const UITextVisual& visual,
    Engine::DrawList2D& output, UIRenderResult& result) const {
    if (!m_TextLayouts || !visual.layout.IsValid() || !Finite(visual.color)) {
        ++result.missingTextLayoutCount; return false;
    }
    const Engine::TextLayoutBitmap* bitmap = m_TextLayouts->GetBitmap(visual.layout);
    if (!bitmap || bitmap->width <= 0 || bitmap->height <= 0 || bitmap->bgraPremultiplied.empty()) {
        ++result.missingTextLayoutCount; return false;
    }
    const Engine::RectF bounds = LocalRect(node);
    Engine::Vec2F position{};
    if (visual.horizontalAlignment == UITextHorizontalAlignment::Center)
        position.x = (bounds.width - static_cast<float>(bitmap->width)) * 0.5f;
    else if (visual.horizontalAlignment == UITextHorizontalAlignment::End)
        position.x = bounds.width - static_cast<float>(bitmap->width);
    if (visual.verticalAlignment == UITextVerticalAlignment::Center)
        position.y = (bounds.height - static_cast<float>(bitmap->height)) * 0.5f;
    else if (visual.verticalAlignment == UITextVerticalAlignment::End)
        position.y = bounds.height - static_cast<float>(bitmap->height);
    Engine::TextCommand command;
    command.position = position; command.layout = visual.layout;
    command.color = ResolveColor(visual.color, node.layoutState.effectiveOpacity, Engine::AlphaMode::Premultiplied,
        ResolveAnimatedProperties(node).colorMultiplier);
    command.transform = node.layoutState.computedTransform.localToScene;
    output.AddText(command);
    return true;
}

bool BasicUIRenderBuilder::EmitNineSlice(
    const UINodeRecord& node, const UINineSliceVisual& visual,
    Engine::DrawList2D& output, UIRenderResult& result) const {
    const Engine::RectF rect = LocalRect(node);
    Engine::TextureInfo info;
    info.state = Engine::TextureState::Missing;
    if (m_Textures) info = m_Textures->GetInfo(visual.texture);
    if (info.state != Engine::TextureState::Ready || info.width <= 0 || info.height <= 0)
        return EmitTexturePlaceholder(node, info.state, output, result);
    if (!PositiveRect(rect) || !Finite(visual.uv) || !Finite(visual.tint)) {
        ++result.invalidVisualCount; return false;
    }
    const Engine::InsetsF source = SelectBorders(visual.sourceBorders, visual.borders);
    const Engine::InsetsF destination = NormalizeDestinationBorders(
        SelectBorders(visual.destinationBorders, visual.borders), rect);
    Engine::NineSliceCommand command;
    command.dst = rect; command.uv = visual.uv; command.texture = visual.texture;
    command.left = destination.left; command.right = destination.right;
    command.top = destination.top; command.bottom = destination.bottom;
    command.sourceLeftUv = std::clamp(source.left / static_cast<float>(info.width), 0.0f, 1.0f);
    command.sourceRightUv = std::clamp(source.right / static_cast<float>(info.width), 0.0f, 1.0f);
    command.sourceTopUv = std::clamp(source.top / static_cast<float>(info.height), 0.0f, 1.0f);
    command.sourceBottomUv = std::clamp(source.bottom / static_cast<float>(info.height), 0.0f, 1.0f);
    const float availableU = std::abs(command.uv.width);
    const float sourceHorizontal = command.sourceLeftUv + command.sourceRightUv;
    if (sourceHorizontal > availableU && sourceHorizontal > 0.0f) {
        const float scale = availableU / sourceHorizontal;
        command.sourceLeftUv *= scale; command.sourceRightUv *= scale;
    }
    const float availableV = std::abs(command.uv.height);
    const float sourceVertical = command.sourceTopUv + command.sourceBottomUv;
    if (sourceVertical > availableV && sourceVertical > 0.0f) {
        const float scale = availableV / sourceVertical;
        command.sourceTopUv *= scale; command.sourceBottomUv *= scale;
    }
    command.alphaMode = info.alphaMode;
    command.color = ResolveColor(visual.tint, node.layoutState.effectiveOpacity, info.alphaMode,
        ResolveAnimatedProperties(node).colorMultiplier);
    command.transform = node.layoutState.computedTransform.localToScene;
    output.AddNineSlice(command);
    return true;
}

bool BasicUIRenderBuilder::EmitShape(
    const UINodeRecord& node, const UIShapeVisual& visual,
    Engine::DrawList2D& output, UIRenderResult& result) const {
    if (!Finite(visual.color)) { ++result.invalidVisualCount; return false; }
    const Engine::RectF rect = LocalRect(node);
    const Engine::Color4f color = ResolveColor(
        visual.color, node.layoutState.effectiveOpacity, Engine::AlphaMode::Premultiplied,
        ResolveAnimatedProperties(node).colorMultiplier);
    const Engine::Mat3F transform = node.layoutState.computedTransform.localToScene;
    if (visual.type == UIShapeType::Line) {
        Engine::Vec2F end = visual.lineEnd;
        if (end == Engine::Vec2F{} && visual.lineTo != Engine::Vec2F{}) end = visual.lineTo;
        const float thickness = std::max(0.0f, Finite(visual.thickness) ? visual.thickness : 0.0f);
        if (!Finite(visual.lineStart) || !Finite(end) || thickness <= 0.0f) {
            ++result.invalidVisualCount; return false;
        }
        Engine::LineCommand command;
        command.from = visual.lineStart; command.to = end; command.thickness = thickness;
        command.color = color; command.transform = transform; output.AddLine(command); return true;
    }
    if (!PositiveRect(rect)) return false;
    if (visual.type == UIShapeType::SolidRect) {
        Engine::SolidRectCommand command; command.rect = rect; command.color = color; command.transform = transform;
        output.AddSolidRect(command); return true;
    }
    if (visual.type == UIShapeType::RoundedRect) {
        const float authored = visual.cornerRadius > 0.0f ? visual.cornerRadius : visual.radius;
        Engine::RoundedRectCommand command; command.rect = rect;
        command.radius = std::clamp(Finite(authored) ? authored : 0.0f, 0.0f, std::min(rect.width, rect.height) * 0.5f);
        command.color = color; command.transform = transform; output.AddRoundedRect(command); return true;
    }
    const Engine::Vec2F center{rect.width * 0.5f, rect.height * 0.5f};
    const float maximum = std::min(rect.width, rect.height) * 0.5f;
    if (visual.type == UIShapeType::Circle) {
        const float authored = visual.outerRadius > 0.0f ? visual.outerRadius : visual.radius;
        Engine::CircleCommand command; command.center = center;
        command.radius = authored > 0.0f && Finite(authored) ? std::min(authored, maximum) : maximum;
        command.color = color; command.transform = transform; output.AddCircle(command); return true;
    }
    if (visual.type == UIShapeType::Ring) {
        const float authoredOuter = visual.outerRadius > 0.0f ? visual.outerRadius : visual.radius;
        const float outer = authoredOuter > 0.0f && Finite(authoredOuter) ? std::min(authoredOuter, maximum) : maximum;
        const float authoredInner = visual.innerRadius > 0.0f ? visual.innerRadius : outer - visual.thickness;
        const float inner = std::clamp(Finite(authoredInner) ? authoredInner : 0.0f, 0.0f, outer);
        Engine::RingCommand command; command.center = center; command.radius = outer;
        command.thickness = outer - inner; command.color = color; command.transform = transform;
        output.AddRing(command); return true;
    }
    ++result.invalidVisualCount;
    return false;
}

} // namespace Engine::UI2D
