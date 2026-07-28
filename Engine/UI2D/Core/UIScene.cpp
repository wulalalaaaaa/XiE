#include "UI2D/Core/UIScene.h"

#include <cmath>

#include <algorithm>

namespace Engine::UI2D {

UIScene::UIScene() {
    m_Root = m_Store.Create("Root");
    if (UINodeRecord* root = TryGet(m_Root)) {
        root->insertionOrder = m_NextInsertionOrder++;
        root->hitTestVisible = false;
        root->focus.focusable = false;
    }
    RebuildPainterTraversal();
}

UINodeHandle UIScene::CreateNode(std::string debugName, UINodeHandle parent) {
    if (!parent.IsValid()) parent = m_Root;
    if (TryGet(parent) == nullptr) return {};
    const UINodeHandle handle = m_Store.Create(std::move(debugName));
    UINodeRecord* node = TryGet(handle);
    UINodeRecord* parentNode = TryGet(parent);
    node->insertionOrder = m_NextInsertionOrder++;
    node->parent = parent;
    parentNode->children.push_back(handle);
    MarkDirty(parent, UIDirtyFlags::Children | UIDirtyFlags::Layout | UIDirtyFlags::HitTest);
    MarkStyleDirty(handle);
    ++m_FocusRevision;
    return handle;
}

bool UIScene::DestroyNode(UINodeHandle handle) {
    if (handle == m_Root || TryGet(handle) == nullptr) return false;
    RemoveFromParent(handle);
    DestroySubtree(handle);
    m_SceneDirty |= UIDirtyFlags::All;
    ++m_HitTestRevision;
    ++m_FocusRevision;
    RebuildPainterTraversal();
    return true;
}

bool UIScene::ReparentNode(UINodeHandle child, UINodeHandle parent) {
    if (child == m_Root || child == parent || TryGet(child) == nullptr || TryGet(parent) == nullptr ||
        WouldCreateCycle(child, parent)) return false;
    RemoveFromParent(child);
    UINodeRecord* childNode = TryGet(child);
    UINodeRecord* parentNode = TryGet(parent);
    childNode->parent = parent;
    parentNode->children.push_back(child);
    MarkDirty(child, UIDirtyFlags::Transform | UIDirtyFlags::Layout | UIDirtyFlags::HitTest);
    MarkDirty(parent, UIDirtyFlags::Children | UIDirtyFlags::Layout | UIDirtyFlags::HitTest);
    ++m_FocusRevision;
    MarkStyleSubtreeDirty(child);
    return true;
}

bool UIScene::SetLayout(UINodeHandle handle, const UILayoutParams& layout) {
    UINodeRecord* node = TryGet(handle);
    if (!node) return false;
    if (node->layout == layout) return true;
    node->layout = layout;
    return MarkDirty(handle, UIDirtyFlags::Layout | UIDirtyFlags::HitTest);
}

bool UIScene::SetTransform(UINodeHandle handle, const UITransform& transform) {
    UINodeRecord* node = TryGet(handle);
    if (!node) return false;
    if (node->transform == transform) return true;
    node->transform = transform;
    return MarkDirty(handle,
        UIDirtyFlags::Layout | UIDirtyFlags::Transform | UIDirtyFlags::Visual | UIDirtyFlags::HitTest);
}

bool UIScene::SetVisual(UINodeHandle handle, UIVisual visual) {
    UINodeRecord* node = TryGet(handle);
    if (!node) return false;
    node->visual = std::move(visual);
    UIDirtyFlags flags = UIDirtyFlags::Visual;
    if (node->layout.sizeRule.width.mode != UISizeMode::Fixed ||
        node->layout.sizeRule.height.mode != UISizeMode::Fixed)
        flags |= UIDirtyFlags::Layout;
    return MarkDirty(handle, flags);
}

bool UIScene::SetVisibility(UINodeHandle handle, bool visible) {
    UINodeRecord* node = TryGet(handle);
    if (!node) return false;
    if (node->visible == visible) return true;
    node->visible = visible;
    ++m_FocusRevision;
    return MarkDirty(handle, UIDirtyFlags::Layout | UIDirtyFlags::Visual | UIDirtyFlags::HitTest);
}

bool UIScene::SetEnabled(UINodeHandle handle, bool enabled) {
    UINodeRecord* node = TryGet(handle);
    if (!node) return false;
    if (node->enabled == enabled) return true;
    node->enabled = enabled;
    ++m_FocusRevision;
    MarkStyleSubtreeDirty(handle);
    return MarkDirty(handle, UIDirtyFlags::Layout | UIDirtyFlags::Visual | UIDirtyFlags::HitTest);
}

bool UIScene::SetStyleClass(UINodeHandle handle, UIStyleClassId styleClass) {
    UINodeRecord* node = TryGet(handle);
    if (!node) return false;
    if (node->style.reference.styleClass == styleClass) return true;
    node->style.reference.styleClass = styleClass;
    MarkStyleDirty(handle);
    return true;
}

bool UIScene::SetExplicitInteractionStates(UINodeHandle handle, UIInteractionStateMask states) {
    UINodeRecord* node = TryGet(handle);
    if (!node) return false;
    if (node->style.explicitStates == states) return true;
    node->style.explicitStates = states;
    MarkStyleDirty(handle);
    return true;
}

bool UIScene::SetWidgetRuntimeOffset(UINodeHandle handle, Engine::Vec2F offset) {
    UINodeRecord* node = TryGet(handle);
    if (!node) return false;
    if (!std::isfinite(offset.x)) offset.x = 0.0f;
    if (!std::isfinite(offset.y)) offset.y = 0.0f;
    if (node->widgetRuntime.positionOffset == offset) return true;
    node->widgetRuntime.positionOffset = offset;
    return MarkDirty(handle, UIDirtyFlags::Transform | UIDirtyFlags::Visual | UIDirtyFlags::HitTest);
}

Engine::Vec2F UIScene::GetWidgetRuntimeOffset(UINodeHandle handle) const {
    const UINodeRecord* node = TryGet(handle);
    return node ? node->widgetRuntime.positionOffset : Engine::Vec2F{};
}

UIInteractionStateMask UIScene::GetExplicitInteractionStates(UINodeHandle handle) const {
    const UINodeRecord* node = TryGet(handle);
    return node ? node->style.explicitStates : 0;
}

UIStyleClassId UIScene::GetStyleClass(UINodeHandle handle) const {
    const UINodeRecord* node = TryGet(handle);
    return node ? node->style.reference.styleClass : UIStyleClassId{};
}

const UIStyleRuntimeProperties* UIScene::TryGetRuntimeStyle(UINodeHandle handle) const {
    const UINodeRecord* node = TryGet(handle);
    return node ? &node->style.runtimeProperties : nullptr;
}

void UIScene::MarkStyleDirty(UINodeHandle node) {
    if (!TryGet(node)) return;
    m_StyleDirty.MarkDirty(node);
    ++m_StyleRevision;
}
void UIScene::MarkStyleSubtreeDirty(UINodeHandle root) {
    if (!TryGet(root)) return;
    m_StyleDirty.MarkSubtreeDirty(*this, root);
    ++m_StyleRevision;
}
void UIScene::MarkAllStyleDirty() {
    m_StyleDirty.MarkAllDirty(*this);
    ++m_StyleRevision;
}
std::vector<UINodeHandle> UIScene::ConsumeStyleDirtyNodes() { return m_StyleDirty.Consume(); }

bool UIScene::SetFocusable(UINodeHandle handle, bool focusable) {
    UINodeRecord* node = TryGet(handle);
    if (!node) return false;
    if (node->focus.focusable == focusable) return true;
    node->focus.focusable = focusable;
    ++m_FocusRevision;
    return MarkDirty(handle, UIDirtyFlags::Visual);
}

bool UIScene::SetTabIndex(UINodeHandle handle, std::int32_t tabIndex) {
    UINodeRecord* node = TryGet(handle);
    if (!node) return false;
    if (node->focus.tabIndex == tabIndex) return true;
    node->focus.tabIndex = tabIndex;
    ++m_FocusRevision;
    return MarkDirty(handle, UIDirtyFlags::Visual);
}

bool UIScene::SetFocusProperties(UINodeHandle handle, UIFocusProperties properties) {
    UINodeRecord* node = TryGet(handle);
    if (!node) return false;
    if (node->focus == properties) return true;
    node->focus = properties;
    ++m_FocusRevision;
    return MarkDirty(handle, UIDirtyFlags::Visual);
}

bool UIScene::SetHitTestVisible(UINodeHandle handle, bool visible) {
    UINodeRecord* node = TryGet(handle);
    if (!node) return false;
    if (node->hitTestVisible == visible) return true;
    node->hitTestVisible = visible;
    return MarkDirty(handle, UIDirtyFlags::HitTest);
}

bool UIScene::SetHitShape(UINodeHandle handle, const UIHitShape& shape) {
    UINodeRecord* node = TryGet(handle);
    if (!node) return false;
    if (node->hitShape == shape) return true;
    node->hitShape = shape;
    return MarkDirty(handle, UIDirtyFlags::HitTest);
}

bool UIScene::SetClipChildren(UINodeHandle handle, bool clipChildren) {
    UINodeRecord* node = TryGet(handle);
    if (!node) return false;
    if (node->clipChildren == clipChildren) return true;
    node->clipChildren = clipChildren;
    return MarkDirty(handle, UIDirtyFlags::Visual | UIDirtyFlags::HitTest);
}

bool UIScene::SetZOrder(UINodeHandle handle, std::int32_t zOrder) {
    UINodeRecord* node = TryGet(handle);
    if (!node) return false;
    if (node->zOrder == zOrder) return true;
    node->zOrder = zOrder;
    return MarkDirty(handle, UIDirtyFlags::Visual | UIDirtyFlags::HitTest);
}

std::span<const UINodeHandle> UIScene::Children(UINodeHandle parent) const {
    const UINodeRecord* node = TryGet(parent);
    return node ? std::span<const UINodeHandle>(node->children) : std::span<const UINodeHandle>(m_EmptyChildren);
}

bool UIScene::MarkDirty(UINodeHandle handle, UIDirtyFlags flags) {
    UINodeRecord* node = TryGet(handle);
    if (!node) return false;
    node->dirtyFlags |= flags;
    m_SceneDirty |= flags;
    if (HasAny(flags, UIDirtyFlags::Transform | UIDirtyFlags::Visual | UIDirtyFlags::Children))
        ++m_VisualRevision;
    if (HasAny(flags, UIDirtyFlags::HitTest)) {
        ++m_HitTestRevision;
        RebuildPainterTraversal();
    }
    return true;
}

void UIScene::MarkAllDirty(UIDirtyFlags flags) {
    for (UINodeHandle handle : LiveNodes()) TryGet(handle)->dirtyFlags |= flags;
    m_SceneDirty |= flags;
    if (HasAny(flags, UIDirtyFlags::Transform | UIDirtyFlags::Visual | UIDirtyFlags::Children))
        ++m_VisualRevision;
    if (HasAny(flags, UIDirtyFlags::HitTest)) {
        ++m_HitTestRevision;
        RebuildPainterTraversal();
    }
}

void UIScene::ClearDirty(UIDirtyFlags flags) {
    for (UINodeHandle handle : LiveNodes()) TryGet(handle)->dirtyFlags &= ~flags;
    m_SceneDirty &= ~flags;
}

bool UIScene::HasDirty(UIDirtyFlags flags) const { return HasAny(m_SceneDirty, flags); }

void UIScene::SetNodeInvalidatedCallback(std::function<void(UINodeHandle)> callback) {
    m_OnNodeInvalidated = std::move(callback);
}

bool UIScene::WouldCreateCycle(UINodeHandle child, UINodeHandle parent) const {
    UINodeHandle current = parent;
    while (current.IsValid()) {
        if (current == child) return true;
        const UINodeRecord* node = TryGet(current);
        current = node ? node->parent : UINodeHandle{};
    }
    return false;
}

void UIScene::DestroySubtree(UINodeHandle handle) {
    UINodeRecord* node = TryGet(handle);
    if (!node) return;
    const std::vector<UINodeHandle> children = node->children;
    for (UINodeHandle child : children) DestroySubtree(child);
    if (m_OnNodeInvalidated) m_OnNodeInvalidated(handle);
    m_StyleDirty.OnNodeInvalidated(handle);
    (void)m_Store.Destroy(handle);
}

void UIScene::RemoveFromParent(UINodeHandle child) {
    UINodeRecord* node = TryGet(child);
    if (!node || !node->parent.IsValid()) return;
    if (UINodeRecord* parent = TryGet(node->parent)) {
        std::erase(parent->children, child);
        MarkDirty(parent->handle, UIDirtyFlags::Children | UIDirtyFlags::Layout | UIDirtyFlags::HitTest);
    }
    // Preserve lastKnownParent until Reparent overwrites it or destruction
    // invalidation callbacks have captured the old relationship.
}

void UIScene::RebuildPainterTraversal() {
    m_PainterTraversal.clear();
    m_PainterTraversal.reserve(NodeCount());
    AppendPainterSubtree(m_Root, 0);
}

void UIScene::AppendPainterSubtree(UINodeHandle handle, std::uint32_t depth) {
    const UINodeRecord* node = TryGet(handle);
    if (!node) return;
    m_PainterTraversal.push_back({handle, node->parent,
        static_cast<std::uint64_t>(m_PainterTraversal.size()), depth});

    std::vector<UINodeHandle> orderedChildren;
    orderedChildren.reserve(node->children.size());
    for (UINodeHandle child : node->children) if (TryGet(child)) orderedChildren.push_back(child);
    std::stable_sort(orderedChildren.begin(), orderedChildren.end(), [this](UINodeHandle lhs, UINodeHandle rhs) {
        const UINodeRecord* left = TryGet(lhs);
        const UINodeRecord* right = TryGet(rhs);
        if (left->zOrder != right->zOrder) return left->zOrder < right->zOrder;
        return left->insertionOrder < right->insertionOrder;
    });
    for (UINodeHandle child : orderedChildren) AppendPainterSubtree(child, depth + 1);
}

} // namespace Engine::UI2D
