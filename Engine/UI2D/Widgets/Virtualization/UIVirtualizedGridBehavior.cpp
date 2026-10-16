#include "UI2D/Widgets/Virtualization/UIVirtualizedGridBehavior.h"

#include "UI2D/Runtime/UIWindowRuntime.h"
#include "UI2D/Widgets/Core/UIWidgetRecord.h"
#include "UI2D/Widgets/Runtime/UIWidgetRuntime.h"

#include <utility>

namespace Engine::UI2D {

UIVirtualizedGridBehavior::UIVirtualizedGridBehavior(
    UIWindowRuntime& runtime,
    UIWidgetHandle root,
    UIVirtualGridMetrics metrics,
    std::uint64_t itemCount,
    UIVirtualizedGridCallbacks callbacks)
    : m_Runtime(&runtime),
      m_Root(root),
      m_Model(metrics),
      m_Callbacks(std::move(callbacks)) {
    m_State.itemCount = itemCount;
}

UIVirtualizedGridBehavior::~UIVirtualizedGridBehavior() {
    Clear();
}

bool UIVirtualizedGridBehavior::SetItemCount(std::uint64_t itemCount) {
    if (m_State.itemCount == itemCount) return false;
    m_State.itemCount = itemCount;
    if (!m_Runtime || !m_Runtime->IsVisible()) return true;
    return ApplyLayout(m_Model.Calculate(m_State));
}

bool UIVirtualizedGridBehavior::SetMetrics(
    const UIVirtualGridMetrics& metrics) {
    if (!m_Model.SetMetrics(metrics)) return false;
    if (!m_Runtime || !m_Runtime->IsVisible()) return true;
    return ApplyLayout(m_Model.Calculate(m_State));
}

bool UIVirtualizedGridBehavior::RefreshVisibleItems(
    float viewportWidth, float viewportHeight, float scrollOffsetY) {
    m_State.viewportWidth = viewportWidth;
    m_State.viewportHeight = viewportHeight;
    m_State.scrollOffsetY = scrollOffsetY;
    if (!m_Runtime || !m_Runtime->IsVisible()) return false;
    return ApplyLayout(m_Model.Calculate(m_State));
}

void UIVirtualizedGridBehavior::RefreshBindings() {
    for (const auto& [index, slot] : m_Active) {
        if (m_Callbacks.unbindItem)
            m_Callbacks.unbindItem(slot.widget, index);
        if (m_Callbacks.bindItem)
            m_Callbacks.bindItem(slot.widget, index);
    }
}

bool UIVirtualizedGridBehavior::ApplyLayout(
    const UIVirtualGridLayout& next) {
    if (!m_Runtime || !m_Root.IsValid()) return false;
    const bool geometryChanged =
        !m_HasLayout || next.columns != m_Layout.columns ||
        next.contentSize != m_Layout.contentSize;
    const bool rangeChanged =
        !m_HasLayout || next.realized != m_Layout.realized;
    if (!geometryChanged && !rangeChanged &&
        next.itemCount == m_Layout.itemCount) {
        m_Layout.visible = next.visible;
        return false;
    }

    UIWidgetRuntime& widgets = m_Runtime->Widgets();
    if (geometryChanged || next.itemCount != m_Layout.itemCount) {
        UILayoutParams rootLayout;
        rootLayout.mode = UILayoutMode::Absolute;
        rootLayout.sizeRule.width.mode = UISizeMode::Stretch;
        rootLayout.sizeRule.height = {UISizeMode::Fixed, next.contentSize.y};
        (void)widgets.SetLayout(m_Root, rootLayout);
    }

    m_Layout = next;
    for (auto it = m_Active.begin(); it != m_Active.end();) {
        if (!next.realized.Contains(it->first)) {
            auto release = it++;
            Release(release);
        } else {
            ++it;
        }
    }

    if (!next.realized.Empty()) {
        for (std::uint64_t index = next.realized.first;; ++index) {
            if (!m_Active.contains(index)) (void)Acquire(index);
            if (index == next.realized.last) break;
        }
    }

    m_HasLayout = true;
    if (geometryChanged) {
        for (const auto& [index, slot] : m_Active)
            Position(slot.widget, index);
    }
    return true;
}

UIWidgetHandle UIVirtualizedGridBehavior::Acquire(std::uint64_t index) {
    UIWidgetHandle widget;
    UIWidgetRuntime& widgets = m_Runtime->Widgets();
    if (!m_Pool.empty()) {
        widget = m_Pool.back();
        m_Pool.pop_back();
        (void)widgets.SetVisible(widget, true);
    } else if (m_Callbacks.createItem) {
        UIWidgetContext context = widgets.Context();
        widget = m_Callbacks.createItem(context, m_Root);
    }
    if (!widget.IsValid()) return {};
    m_Active.emplace(index, ActiveSlot{widget, index});
    Position(widget, index);
    if (m_Callbacks.bindItem) m_Callbacks.bindItem(widget, index);
    return widget;
}

void UIVirtualizedGridBehavior::Release(
    std::map<std::uint64_t, ActiveSlot>::iterator slot) {
    if (m_Callbacks.unbindItem)
        m_Callbacks.unbindItem(slot->second.widget, slot->first);
    const UIWidgetRecord* record =
        m_Runtime->Widgets().Registry().TryGet(slot->second.widget);
    if (record && m_Runtime->HasFocusWithin(record->rootNode))
        m_Runtime->ClearFocus(UIFocusReason::NodeInvalidated);
    (void)m_Runtime->Widgets().SetVisible(slot->second.widget, false);
    m_Pool.push_back(slot->second.widget);
    m_Active.erase(slot);
}

void UIVirtualizedGridBehavior::Position(
    UIWidgetHandle widget, std::uint64_t index) {
    const auto rect = m_Model.CellRect(index, m_Layout.itemCount == 0
        ? m_Model.Calculate(m_State) : m_Layout);
    if (!rect) return;
    UILayoutParams layout;
    layout.mode = UILayoutMode::Absolute;
    layout.offsetMin = {rect->x, rect->y};
    layout.sizeRule.width = {UISizeMode::Fixed, rect->width};
    layout.sizeRule.height = {UISizeMode::Fixed, rect->height};
    (void)m_Runtime->Widgets().SetLayout(widget, layout);
}

void UIVirtualizedGridBehavior::Clear() {
    if (!m_Runtime) return;
    for (const auto& [index, slot] : m_Active) {
        if (m_Callbacks.unbindItem)
            m_Callbacks.unbindItem(slot.widget, index);
    }
    m_Active.clear();
    m_Pool.clear();
    if (m_Root.IsValid()) (void)m_Runtime->Widgets().Destroy(m_Root);
    m_Root = {};
}

UIVirtualGridSnapshot UIVirtualizedGridBehavior::Snapshot() const noexcept {
    UIVirtualGridSnapshot result;
    static_cast<UIVirtualGridLayout&>(result) = m_Layout;
    result.activeSlots = m_Active.size();
    result.pooledSlots = m_Pool.size();
    return result;
}

} // namespace Engine::UI2D
