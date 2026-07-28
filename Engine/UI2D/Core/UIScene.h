#pragma once

#include "UI2D/Core/UINodeStore.h"
#include "UI2D/Core/UITraversal.h"
#include "UI2D/Style/UIStyleDirtyQueue.h"

#include <functional>
#include <span>

namespace Engine::UI2D {

// Current implementation raises node changes to scene-level dirty state.
// Subtree-granular layout/render rebuild is reserved for a later phase.
class UIScene {
public:
    UIScene();

    UINodeHandle CreateNode(std::string debugName = {}, UINodeHandle parent = {});
    bool DestroyNode(UINodeHandle handle);
    bool ReparentNode(UINodeHandle child, UINodeHandle parent);
    bool SetLayout(UINodeHandle handle, const UILayoutParams& layout);
    bool SetTransform(UINodeHandle handle, const UITransform& transform);
    bool SetVisual(UINodeHandle handle, UIVisual visual);
    bool SetVisibility(UINodeHandle handle, bool visible);
    bool SetEnabled(UINodeHandle handle, bool enabled);
    bool SetFocusable(UINodeHandle handle, bool focusable);
    bool SetTabIndex(UINodeHandle handle, std::int32_t tabIndex);
    bool SetFocusProperties(UINodeHandle handle, UIFocusProperties properties);
    bool SetHitTestVisible(UINodeHandle handle, bool visible);
    bool SetHitShape(UINodeHandle handle, const UIHitShape& shape);
    bool SetClipChildren(UINodeHandle handle, bool clipChildren);
    bool SetZOrder(UINodeHandle handle, std::int32_t zOrder);
    bool SetStyleClass(UINodeHandle handle, UIStyleClassId styleClass);
    bool SetExplicitInteractionStates(UINodeHandle handle, UIInteractionStateMask states);
    bool SetWidgetRuntimeOffset(UINodeHandle handle, Engine::Vec2F offset);
    [[nodiscard]] Engine::Vec2F GetWidgetRuntimeOffset(UINodeHandle handle) const;
    [[nodiscard]] UIInteractionStateMask GetExplicitInteractionStates(UINodeHandle handle) const;
    [[nodiscard]] UIStyleClassId GetStyleClass(UINodeHandle handle) const;
    [[nodiscard]] const UIStyleRuntimeProperties* TryGetRuntimeStyle(UINodeHandle handle) const;

    UINodeRecord* TryGet(UINodeHandle handle) { return m_Store.TryGet(handle); }
    const UINodeRecord* TryGet(UINodeHandle handle) const { return m_Store.TryGet(handle); }
    UINodeHandle Root() const noexcept { return m_Root; }
    std::span<const UINodeHandle> Children(UINodeHandle parent) const;
    std::span<const UITraversalEntry> PainterTraversal() const noexcept { return m_PainterTraversal; }
    std::vector<UINodeHandle> LiveNodes() const { return m_Store.LiveHandles(); }
    std::size_t NodeCount() const noexcept { return m_Store.Size(); }

    bool MarkDirty(UINodeHandle handle, UIDirtyFlags flags);
    void MarkAllDirty(UIDirtyFlags flags);
    void ClearDirty(UIDirtyFlags flags);
    [[nodiscard]] bool HasDirty(UIDirtyFlags flags) const;
    [[nodiscard]] UIDirtyFlags SceneDirtyFlags() const noexcept { return m_SceneDirty; }
    [[nodiscard]] std::uint64_t HitTestRevision() const noexcept { return m_HitTestRevision; }
    [[nodiscard]] std::uint64_t VisualRevision() const noexcept { return m_VisualRevision; }
    [[nodiscard]] std::uint64_t FocusRevision() const noexcept { return m_FocusRevision; }
    void InvalidateFocusRevision() noexcept { ++m_FocusRevision; }
    [[nodiscard]] std::uint64_t StyleRevision() const noexcept { return m_StyleRevision; }
    [[nodiscard]] std::uint64_t LayoutRevision() const noexcept { return m_LayoutRevision; }
    void InvalidateLayoutRevision() noexcept { ++m_LayoutRevision; }
    void MarkStyleDirty(UINodeHandle node);
    void MarkStyleSubtreeDirty(UINodeHandle root);
    void MarkAllStyleDirty();
    std::vector<UINodeHandle> ConsumeStyleDirtyNodes();

    void SetNodeInvalidatedCallback(std::function<void(UINodeHandle)> callback);

private:
    bool WouldCreateCycle(UINodeHandle child, UINodeHandle parent) const;
    void DestroySubtree(UINodeHandle handle);
    void RemoveFromParent(UINodeHandle child);
    void RebuildPainterTraversal();
    void AppendPainterSubtree(UINodeHandle node, std::uint32_t depth);

    UINodeStore m_Store;
    UINodeHandle m_Root{};
    UIDirtyFlags m_SceneDirty = UIDirtyFlags::All;
    std::uint64_t m_NextInsertionOrder = 1;
    std::uint64_t m_HitTestRevision = 1;
    std::uint64_t m_VisualRevision = 1;
    std::uint64_t m_FocusRevision = 1;
    std::uint64_t m_StyleRevision = 1;
    std::uint64_t m_LayoutRevision = 1;
    UIStyleDirtyQueue m_StyleDirty;
    std::vector<UITraversalEntry> m_PainterTraversal;
    std::function<void(UINodeHandle)> m_OnNodeInvalidated;
    std::vector<UINodeHandle> m_EmptyChildren;
};

} // namespace Engine::UI2D
