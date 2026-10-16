#include "UI2D/Widgets/Virtualization/UIVirtualizedGridWidget.h"

#include "UI2D/Runtime/UIWindowRuntime.h"
#include "UI2D/Widgets/Basic/UIPanelWidget.h"
#include "UI2D/Widgets/Runtime/UIWidgetRuntime.h"

namespace Engine::UI2D {

UIVirtualizedGridWidget::UIVirtualizedGridWidget(
    UIWindowRuntime& runtime,
    UIWidgetHandle scrollView,
    UIWidgetConnectionHandle scrollConnection,
    std::unique_ptr<UIVirtualizedGridBehavior> behavior)
    : m_Runtime(&runtime),
      m_ScrollView(scrollView),
      m_ScrollConnection(scrollConnection),
      m_Behavior(std::move(behavior)) {}

UIVirtualizedGridWidget::~UIVirtualizedGridWidget() {
    if (m_Runtime && m_ScrollConnection.IsValid())
        (void)m_Runtime->Widgets().Disconnect(m_ScrollConnection);
}

std::unique_ptr<UIVirtualizedGridWidget> UIVirtualizedGridWidget::Create(
    UIWidgetContext& context,
    const UIVirtualizedGridWidgetDesc& desc,
    UIWidgetHandle scrollView) {
    if (!scrollView.IsValid() ||
        !UIVirtualGridModel::IsValidMetrics(desc.metrics) ||
        !desc.callbacks.createItem) {
        return {};
    }
    UIPanelWidgetDesc root;
    root.common = desc.common;
    root.common.layout.mode = UILayoutMode::Absolute;
    root.common.layout.sizeRule.width.mode = UISizeMode::Stretch;
    root.common.layout.sizeRule.height = {UISizeMode::Fixed, 0.0f};
    root.backgroundColor = desc.backgroundColor;
    const UIWidgetHandle rootHandle =
        UIPanelWidget::Create(context, root);
    if (!rootHandle.IsValid() ||
        !context.windowRuntime.Widgets().AddContent(scrollView, rootHandle)) {
        if (rootHandle.IsValid())
            (void)context.windowRuntime.Widgets().Destroy(rootHandle);
        return {};
    }
    auto behavior = std::make_unique<UIVirtualizedGridBehavior>(
        context.windowRuntime, rootHandle, desc.metrics,
        desc.itemCount, desc.callbacks);
    auto result = std::unique_ptr<UIVirtualizedGridWidget>(
        new UIVirtualizedGridWidget(
            context.windowRuntime, scrollView, {}, std::move(behavior)));
    UIVirtualizedGridWidget* weak = result.get();
    result->m_ScrollConnection =
        context.windowRuntime.Widgets().OnScrollChanged(
            scrollView, [weak](const UIScrollChangedEvent&) {
                if (weak) (void)weak->RefreshVisibleItems();
            });
    (void)result->RefreshVisibleItems();
    return result;
}

bool UIVirtualizedGridWidget::SetItemCount(std::uint64_t itemCount) {
    return m_Behavior && m_Behavior->SetItemCount(itemCount);
}

bool UIVirtualizedGridWidget::SetMetrics(
    const UIVirtualGridMetrics& metrics) {
    return m_Behavior && m_Behavior->SetMetrics(metrics);
}

bool UIVirtualizedGridWidget::RefreshVisibleItems() {
    if (!m_Runtime || !m_Behavior) return false;
    const UIScrollSnapshot scroll =
        m_Runtime->Widgets().GetScrollSnapshot(m_ScrollView);
    return m_Behavior->RefreshVisibleItems(
        scroll.viewportSize.x, scroll.viewportSize.y, scroll.offset.y);
}

void UIVirtualizedGridWidget::RefreshBindings() {
    if (m_Behavior) m_Behavior->RefreshBindings();
}

UIVirtualGridSnapshot UIVirtualizedGridWidget::Snapshot() const noexcept {
    return m_Behavior ? m_Behavior->Snapshot() : UIVirtualGridSnapshot{};
}

UIWidgetHandle UIVirtualizedGridWidget::Root() const noexcept {
    return m_Behavior ? m_Behavior->Root() : UIWidgetHandle{};
}

} // namespace Engine::UI2D
