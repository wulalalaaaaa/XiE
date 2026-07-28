#include "UI2D/HitTest/BasicUIHitTester.h"

#include "Foundation/Math/Matrix3.h"
#include "UI2D/Animation/UIAnimatedProperties.h"
#include "UI2D/Core/UIScene.h"
#include "UI2D/HitTest/UIHitGeometry.h"
#include "UI2D/Rendering/UIRenderGeometry.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace Engine::UI2D {
namespace {

Engine::RectF LocalRect(const UINodeRecord& node) noexcept {
    const Engine::Vec2F size = ResolveAnimatedProperties(node).visualSize;
    return {0.0f, 0.0f, size.x, size.y};
}

bool Finite(Engine::Vec2F value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

} // namespace

bool BasicUIHitTester::IsNodeEligible(
    const UINodeRecord& node, bool includeDisabled, UIHitTestStats& stats) const {
    const Engine::RectF localRect = LocalRect(node);
    if (!node.handle.IsValid() || !node.visible || !node.layoutState.effectiveVisible ||
        (!includeDisabled && (!node.enabled || !node.layoutState.effectiveEnabled)) ||
        !node.hitTestVisible || node.hitShape.type == UIHitShapeType::None ||
        !node.layoutState.arrangeValid || !std::isfinite(localRect.width) ||
        !std::isfinite(localRect.height) || localRect.width <= 0.0f || localRect.height <= 0.0f) {
        return false;
    }
    if (!node.layoutState.computedTransform.inverseValid) {
        ++stats.invalidTransformRejected;
        return false;
    }
    return true;
}

bool BasicUIHitTester::PassesAncestorClips(
    const UIScene& scene,
    UINodeHandle handle,
    UINodeHandle searchRoot,
    Engine::Vec2F scenePosition,
    UIHitTestStats& stats) const {
    if (handle == searchRoot) return true;
    const UINodeRecord* node = scene.TryGet(handle);
    UINodeHandle ancestorHandle = node ? node->parent : UINodeHandle{};
    while (ancestorHandle.IsValid()) {
        const UINodeRecord* ancestor = scene.TryGet(ancestorHandle);
        if (!ancestor) return false;
        if (ancestor->clipChildren) {
            if (!PointInRect(scenePosition, ResolveSceneClipRect(*ancestor))) return false;
        }
        if (ancestorHandle == searchRoot) return true;
        ancestorHandle = ancestor->parent;
    }
    return false;
}

bool BasicUIHitTester::HitNodeGeometry(
    const UINodeRecord& node,
    Engine::Vec2F scenePosition,
    Engine::Vec2F& outLocalPosition) const {
    outLocalPosition = Engine::TransformPoint(node.layoutState.computedTransform.sceneToLocal, scenePosition);
    return PointInHitShape(outLocalPosition, LocalRect(node), node.hitShape);
}

bool BasicUIHitTester::BuildRoute(
    const UIScene& scene,
    UINodeHandle searchRoot,
    UINodeHandle target,
    std::vector<UINodeHandle>& output) const {
    output.clear();
    UINodeHandle current = target;
    while (current.IsValid()) {
        const UINodeRecord* node = scene.TryGet(current);
        if (!node) {
            output.clear();
            return false;
        }
        output.push_back(current);
        if (current == searchRoot) {
            std::reverse(output.begin(), output.end());
            return output.back() == target;
        }
        current = node->parent;
    }
    output.clear();
    return false;
}

UIHitTestResult BasicUIHitTester::HitTest(
    const UIScene& scene,
    const UIHitTestContext& context,
    Engine::Vec2F scenePosition) const {
    UIHitTestResult result;
    result.scenePosition = scenePosition;
    UIHitTestStats stats{};
    std::ostringstream debug;
    if (context.debugDumpEnabled) debug << "[HitTest] scene=(" << scenePosition.x << ',' << scenePosition.y << ")\n";

    const UINodeHandle root = context.root.IsValid() ? context.root : scene.Root();
    const UINodeRecord* rootNode = scene.TryGet(root);
    const std::span<const UITraversalEntry> traversal = scene.PainterTraversal();
    if (!Finite(scenePosition) || !rootNode || scene.HasDirty(UIDirtyFlags::Layout) || traversal.empty()) {
        m_LastStats = stats;
        if (context.stats) *context.stats = stats;
        m_LastDebugDump = context.debugDumpEnabled ? debug.str() : std::string{};
        return result;
    }

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
        m_LastStats = stats;
        if (context.stats) *context.stats = stats;
        m_LastDebugDump = context.debugDumpEnabled ? debug.str() : std::string{};
        return result;
    }

    for (std::size_t i = end; i-- > begin;) {
        ++stats.visitedNodes;
        const UITraversalEntry& entry = traversal[i];
        const UINodeRecord* node = scene.TryGet(entry.node);
        if (!node || !IsNodeEligible(*node, context.includeDisabled, stats)) continue;
        const bool aabbPass = PointInRect(scenePosition, node->layoutState.sceneRect);
        if (context.debugDumpEnabled) {
            debug << "[Candidate] " << (node->debugName.empty() ? "<unnamed>" : node->debugName)
                  << " z=" << node->zOrder << " painter=" << entry.painterOrder
                  << " AABB=" << (aabbPass ? "pass" : "reject") << '\n';
        }
        if (!aabbPass) {
            ++stats.aabbRejected;
            continue;
        }
        if (!PassesAncestorClips(scene, entry.node, root, scenePosition, stats)) {
            ++stats.clipRejected;
            if (context.debugDumpEnabled) debug << "[Clip] reject\n";
            continue;
        }
        Engine::Vec2F local{};
        if (!HitNodeGeometry(*node, scenePosition, local)) {
            ++stats.geometryRejected;
            if (context.debugDumpEnabled) debug << "[Geometry] reject\n";
            continue;
        }
        if (!BuildRoute(scene, root, entry.node, result.route)) continue;
        result.target = entry.node;
        result.targetLocalPosition = local;
        result.hit = true;
        if (context.debugDumpEnabled) {
            debug << "[Geometry] pass\n[Result] "
                  << (node->debugName.empty() ? "<unnamed>" : node->debugName) << '\n';
        }
        break;
    }

    m_LastStats = stats;
    if (context.stats) *context.stats = stats;
    m_LastDebugDump = context.debugDumpEnabled ? debug.str() : std::string{};
    return result;
}

} // namespace Engine::UI2D
