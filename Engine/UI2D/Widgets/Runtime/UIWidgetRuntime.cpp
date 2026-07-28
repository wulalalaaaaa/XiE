#include "UI2D/Widgets/Runtime/UIWidgetRuntime.h"

#include "UI2D/Runtime/UIWindowRuntime.h"

#include <limits>

namespace Engine::UI2D {

UIWidgetRuntime::UIWidgetRuntime(UIWindowRuntime& runtime)
    : m_Runtime(&runtime), m_Registry(runtime) {}

UIWidgetContext UIWidgetRuntime::Context() {
    return {*m_Runtime, m_Runtime->Scene(), m_Runtime->EventListeners(), *m_Runtime->Theme()};
}
UIWidgetUpdateResult UIWidgetRuntime::FlushMutations(UIWindowRuntime& runtime) {
    return m_Mutations.Flush(m_Registry, runtime);
}
void UIWidgetRuntime::OnNodeInvalidated(UINodeHandle node) {
    const UIWidgetHandle owner = m_Registry.OwnerOf(node);
    m_Registry.OnNodeInvalidated(node);
    UIWidgetRecord* record = m_Registry.TryGet(owner);
    if (record && record->lifecycle == UIWidgetLifecycleState::Mounted && !record->invalidationQueued) {
        (void)Destroy(owner);
    }
}
void UIWidgetRuntime::Clear(UIWindowRuntime& runtime) {
    m_Mutations.Clear();
    m_Registry.Clear(runtime);
    m_ScrollViews.clear();
    m_Scrollbars.clear();
}

UINodeHandle UIWidgetRuntime::ParentNode(UIWidgetHandle parent) const {
    if (!parent.IsValid()) return m_Runtime->Scene().Root();
    const UIWidgetRecord* record = m_Registry.TryGet(parent);
    if (!record) return {std::numeric_limits<std::uint32_t>::max(),
        std::numeric_limits<std::uint32_t>::max()};
    return record->childHostNode.IsValid() ? record->childHostNode : record->rootNode;
}
bool UIWidgetRuntime::ApplyCommon(UINodeHandle node, const UIWidgetCommonProperties& common) {
    UIScene& scene = m_Runtime->Scene();
    return scene.SetVisibility(node, common.visible) && scene.SetEnabled(node, common.enabled) &&
        scene.SetFocusProperties(node, {common.focusable, common.tabIndex}) &&
        scene.SetLayout(node, common.layout) && scene.SetTransform(node, common.transform) &&
        scene.SetStyleClass(node, common.styleClass);
}
UIWidgetHandle UIWidgetRuntime::RegisterOrRollback(UIWidgetRecord record) {
    const UINodeHandle root = record.rootNode;
    const UIWidgetHandle handle = m_Registry.Register(std::move(record));
    if (!handle.IsValid() && m_Runtime->Scene().TryGet(root)) (void)m_Runtime->Scene().DestroyNode(root);
    return handle;
}

bool UIWidgetRuntime::Destroy(UIWidgetHandle widget) {
    UIWidgetRecord* record = m_Registry.TryGet(widget);
    if (!record || record->invalidationQueued) return false;
    if (record->scrollbar) {
        if (UIWidgetRecord* view = m_Registry.TryGet(record->scrollbar->boundScrollView)) {
            if (view->scrollView) {
                if (view->scrollView->verticalScrollbar == widget)
                    view->scrollView->verticalScrollbar = {};
                if (view->scrollView->horizontalScrollbar == widget)
                    view->scrollView->horizontalScrollbar = {};
            }
        }
        record->scrollbar->boundScrollView = {};
        record->scrollbar->dragging = false;
    }
    if (record->scrollView) {
        for (UIWidgetHandle barHandle : {record->scrollView->verticalScrollbar,
                 record->scrollView->horizontalScrollbar}) {
            UIWidgetRecord* bar = m_Registry.TryGet(barHandle);
            if (bar && bar->scrollbar) {
                bar->scrollbar->boundScrollView = {};
                bar->scrollbar->dragging = false;
            }
        }
        record->scrollView->drag = {};
        record->scrollView->smooth.active = false;
        record->scrollView->inertia.active = false;
    }
    record->invalidationQueued = true;
    m_Mutations.Enqueue(DestroyWidgetMutation{widget});
    return true;
}
bool UIWidgetRuntime::Reparent(UIWidgetHandle child, UIWidgetHandle parent) {
    if (!m_Registry.TryGet(child) || (parent.IsValid() && !m_Registry.TryGet(parent))) return false;
    m_Mutations.Enqueue(ReparentWidgetMutation{child, parent}); return true;
}
bool UIWidgetRuntime::SetVisible(UIWidgetHandle widget, bool visible) {
    UIWidgetRecord* record = m_Registry.TryGet(widget);
    if (!record) return false;
    record->visible = visible;
    if (!visible && record->scrollView) {
        record->scrollView->drag = {};
        record->scrollView->velocitySampleCount = 0;
        SetScrollingStyle(*record, false);
    }
    if (!visible && record->scrollbar) record->scrollbar->dragging = false;
    m_Mutations.Enqueue(SetWidgetVisibleMutation{widget, visible}); return true;
}
bool UIWidgetRuntime::SetEnabled(UIWidgetHandle widget, bool enabled) {
    UIWidgetRecord* record = m_Registry.TryGet(widget);
    if (!record) return false;
    record->enabled = enabled;
    if (!enabled && record->scrollView) {
        record->scrollView->drag = {};
        record->scrollView->velocitySampleCount = 0;
        record->scrollView->inertia.active = false;
        record->scrollView->smooth.active = false;
        SetScrollingStyle(*record, false);
    }
    if (!enabled && record->scrollbar) record->scrollbar->dragging = false;
    m_Mutations.Enqueue(SetWidgetEnabledMutation{widget, enabled}); return true;
}
bool UIWidgetRuntime::SetFocusable(UIWidgetHandle widget, bool value) {
    UIWidgetRecord* record = m_Registry.TryGet(widget);
    return record && m_Runtime->Scene().SetFocusable(record->rootNode, value);
}
bool UIWidgetRuntime::SetTabIndex(UIWidgetHandle widget, std::int32_t value) {
    UIWidgetRecord* record = m_Registry.TryGet(widget);
    return record && m_Runtime->Scene().SetTabIndex(record->rootNode, value);
}
bool UIWidgetRuntime::SetLayout(UIWidgetHandle widget, const UILayoutParams& value) {
    UIWidgetRecord* record = m_Registry.TryGet(widget);
    return record && m_Runtime->Scene().SetLayout(record->rootNode, value);
}
bool UIWidgetRuntime::SetTransform(UIWidgetHandle widget, const UITransform& value) {
    UIWidgetRecord* record = m_Registry.TryGet(widget);
    return record && m_Runtime->Scene().SetTransform(record->rootNode, value);
}
bool UIWidgetRuntime::SetStyleClass(UIWidgetHandle widget, UIStyleClassId value) {
    UIWidgetRecord* record = m_Registry.TryGet(widget);
    return record && m_Runtime->SetStyleClass(record->rootNode, value);
}

UIWidgetConnectionHandle UIWidgetRuntime::Connect(UIWidgetHandle widget, UINodeHandle node,
    UIEventType type, UIEventPhaseMask phases, UIEventCallback callback) {
    return m_Registry.Connect(widget, node, type, phases, std::move(callback));
}
} // namespace Engine::UI2D
