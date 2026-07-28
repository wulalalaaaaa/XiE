#include "UI2D/Widgets/Scrolling/UIScrollbarWidget.h"

#include "UI2D/Runtime/UIWindowRuntime.h"
#include "UI2D/Widgets/Runtime/UIWidgetRuntime.h"

#include <algorithm>
#include <memory>

namespace Engine::UI2D {

UIWidgetHandle UIScrollbarWidget::Create(
    UIWidgetContext& context, const UIScrollbarWidgetDesc& desc, UIWidgetHandle parent) {
    return context.windowRuntime.Widgets().CreateScrollbar(desc, parent);
}

UIWidgetHandle UIWidgetRuntime::CreateScrollbar(
    const UIScrollbarWidgetDesc& desc, UIWidgetHandle parent) {
    UIScene& scene = m_Runtime->Scene();
    const UINodeHandle root = scene.CreateNode("ScrollbarWidget", ParentNode(parent));
    if (!root.IsValid() || !ApplyCommon(root, desc.common)) return {};
    (void)scene.SetVisual(root, UIPanelVisual{desc.trackColor, 0.0f, false});
    (void)scene.SetStyleClass(root, desc.trackStyle);

    const UINodeHandle thumb = scene.CreateNode("ScrollbarThumb", root);
    if (!thumb.IsValid()) {
        (void)scene.DestroyNode(root);
        return {};
    }
    UILayoutParams thumbLayout;
    thumbLayout.mode = UILayoutMode::Absolute;
    thumbLayout.sizeRule.width = {UISizeMode::Fixed, 1.0f};
    thumbLayout.sizeRule.height = {UISizeMode::Fixed, 1.0f};
    (void)scene.SetLayout(thumb, thumbLayout);
    (void)scene.SetFocusProperties(thumb, {false, -1});
    (void)scene.SetStyleClass(thumb, desc.thumbStyle);
    (void)scene.SetVisual(thumb, UIPanelVisual{desc.thumbColor, 0.0f, false});

    UIWidgetRecord record;
    record.parentWidget = parent;
    record.rootNode = root;
    record.childHostNode = root;
    record.ownedNodes = {root, thumb};
    record.kind = UIWidgetKind::Scrollbar;
    record.visible = desc.common.visible;
    record.enabled = desc.common.enabled;
    record.scrollbar = std::make_unique<UIScrollbarState>();
    record.scrollbar->axis = desc.axis;
    record.scrollbar->thumbNode = thumb;
    record.scrollbar->minimumThumbLength = std::max(1.0f, desc.minimumThumbLength);
    const UIWidgetHandle handle = RegisterOrRollback(std::move(record));
    if (!handle.IsValid()) return {};
    m_Scrollbars.push_back(handle);
    InstallScrollbarBehavior(handle);
    return handle;
}

} // namespace Engine::UI2D
