#include "UI2D/Widgets/Scrolling/UIScrollViewWidget.h"

#include "UI2D/Runtime/UIWindowRuntime.h"
#include "UI2D/Widgets/Basic/UIPanelWidget.h"
#include "UI2D/Widgets/Runtime/UIWidgetRuntime.h"

#include <algorithm>
#include <memory>

namespace Engine::UI2D {
namespace {
UILayoutParams FullAnchorLayout() {
    UILayoutParams layout;
    layout.mode = UILayoutMode::Absolute;
    layout.sizeRule.width.mode = UISizeMode::Stretch;
    layout.sizeRule.height.mode = UISizeMode::Stretch;
    layout.anchor = {0.0f, 0.0f, 1.0f, 1.0f};
    return layout;
}
}

UIWidgetHandle UIScrollViewWidget::Create(
    UIWidgetContext& context, const UIScrollViewWidgetDesc& desc, UIWidgetHandle parent) {
    return context.windowRuntime.Widgets().CreateScrollView(desc, parent);
}

UIWidgetHandle UIWidgetRuntime::CreateScrollView(
    const UIScrollViewWidgetDesc& desc, UIWidgetHandle parent) {
    UIScene& scene = m_Runtime->Scene();
    const UINodeHandle root = scene.CreateNode("ScrollViewWidget", ParentNode(parent));
    if (!root.IsValid()) return {};

    UIWidgetCommonProperties common = desc.common;
    common.layout.mode = UILayoutMode::Anchor;
    if (!ApplyCommon(root, common)) {
        (void)scene.DestroyNode(root);
        return {};
    }
    (void)scene.SetVisual(root, UIPanelVisual{desc.backgroundColor, 0.0f, false});

    const UINodeHandle viewport = scene.CreateNode("ScrollViewViewport", root);
    if (!viewport.IsValid()) {
        (void)scene.DestroyNode(root);
        return {};
    }
    UIWidgetCommonProperties viewportCommon;
    viewportCommon.layout = FullAnchorLayout();
    viewportCommon.styleClass = desc.viewportStyle;
    if (!ApplyCommon(viewport, viewportCommon)) {
        (void)scene.DestroyNode(root);
        return {};
    }
    (void)scene.SetClipChildren(viewport, true);
    (void)scene.SetVisual(viewport, UIPanelVisual{{0, 0, 0, 0}, 0.0f, false});

    UIWidgetRecord record;
    record.parentWidget = parent;
    record.rootNode = root;
    record.childHostNode = root;
    record.ownedNodes = {root, viewport};
    record.kind = UIWidgetKind::ScrollView;
    record.visible = desc.common.visible;
    record.enabled = desc.common.enabled;
    record.scrollView = std::make_unique<UIScrollViewState>();
    record.scrollView->viewportNode = viewport;
    record.scrollView->model.SetPolicy(desc.policy);
    record.scrollView->bringFocusedNodeIntoView = desc.bringFocusedNodeIntoView;
    const UIWidgetHandle handle = RegisterOrRollback(std::move(record));
    if (!handle.IsValid()) return {};
    m_ScrollViews.push_back(handle);

    UIPanelWidgetDesc content;
    content.common.layout = desc.contentLayout;
    if (desc.policy.horizontal == UIScrollMode::Disabled)
        content.common.layout.sizeRule.width.mode = UISizeMode::Stretch;
    if (desc.policy.vertical == UIScrollMode::Disabled)
        content.common.layout.sizeRule.height.mode = UISizeMode::Stretch;
    content.backgroundColor = {0, 0, 0, 0};
    const UIWidgetHandle contentHost = CreatePanel(content, handle);
    UIWidgetRecord* current = m_Registry.TryGet(handle);
    const UIWidgetRecord* contentRecord = m_Registry.TryGet(contentHost);
    if (!current || !contentRecord || !scene.ReparentNode(contentRecord->rootNode, viewport)) {
        (void)m_Registry.Destroy(handle, *m_Runtime);
        return {};
    }
    current->scrollView->contentHost = contentHost;

    const float thickness = std::max(1.0f, desc.scrollbarThickness);
    if (desc.createVerticalScrollbar && desc.policy.vertical != UIScrollMode::Disabled) {
        UIScrollbarWidgetDesc bar;
        bar.axis = UIScrollAxis::Vertical;
        bar.minimumThumbLength = desc.minimumThumbLength;
        bar.trackStyle = desc.verticalScrollbarTrackStyle;
        bar.thumbStyle = desc.verticalScrollbarThumbStyle;
        bar.trackColor = desc.scrollbarTrackColor;
        bar.thumbColor = desc.scrollbarThumbColor;
        bar.common.layout.mode = UILayoutMode::Absolute;
        bar.common.layout.anchor = {1.0f, 0.0f, 1.0f, 1.0f};
        bar.common.layout.offsetMin = {-thickness, 0.0f};
        bar.common.layout.sizeRule.width = {UISizeMode::Fixed, thickness};
        bar.common.layout.sizeRule.height.mode = UISizeMode::Stretch;
        const UIWidgetHandle scrollbar = CreateScrollbar(bar, handle);
        if (scrollbar.IsValid()) (void)BindScrollbar(scrollbar, handle, UIScrollAxis::Vertical);
    }
    if (desc.createHorizontalScrollbar && desc.policy.horizontal != UIScrollMode::Disabled) {
        UIScrollbarWidgetDesc bar;
        bar.axis = UIScrollAxis::Horizontal;
        bar.minimumThumbLength = desc.minimumThumbLength;
        bar.trackStyle = desc.horizontalScrollbarTrackStyle;
        bar.thumbStyle = desc.horizontalScrollbarThumbStyle;
        bar.trackColor = desc.scrollbarTrackColor;
        bar.thumbColor = desc.scrollbarThumbColor;
        bar.common.layout.mode = UILayoutMode::Absolute;
        bar.common.layout.anchor = {0.0f, 1.0f, 1.0f, 1.0f};
        bar.common.layout.offsetMin = {0.0f, -thickness};
        bar.common.layout.sizeRule.width.mode = UISizeMode::Stretch;
        bar.common.layout.sizeRule.height = {UISizeMode::Fixed, thickness};
        const UIWidgetHandle scrollbar = CreateScrollbar(bar, handle);
        if (scrollbar.IsValid()) (void)BindScrollbar(scrollbar, handle, UIScrollAxis::Horizontal);
    }
    InstallScrollViewBehavior(handle);
    return handle;
}

} // namespace Engine::UI2D
