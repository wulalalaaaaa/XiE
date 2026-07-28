#include "UI2D/Widgets/Runtime/UIWidgetRuntime.h"

#include "UI2D/Runtime/UIWindowRuntime.h"

#include <algorithm>
#include <cmath>

namespace Engine::UI2D {

UIWidgetHandle UIWidgetRuntime::CreateProgressBar(
    const UIProgressBarWidgetDesc& desc, UIWidgetHandle parent) {
    if (!std::isfinite(desc.minimum) || !std::isfinite(desc.maximum) || desc.minimum >= desc.maximum)
        return {};
    UIScene& scene = m_Runtime->Scene();
    const UINodeHandle root = scene.CreateNode("ProgressBarWidget", ParentNode(parent));
    if (!root.IsValid() || !ApplyCommon(root, desc.common)) return {};
    UILayoutParams rootLayout = desc.common.layout; rootLayout.mode = UILayoutMode::Anchor;
    scene.SetLayout(root, rootLayout); scene.SetHitTestVisible(root, false); scene.SetClipChildren(root, true);
    const UINodeHandle background = scene.CreateNode("ProgressBackground", root);
    const UINodeHandle fill = scene.CreateNode("ProgressFill", root);
    if (!background.IsValid() || !fill.IsValid()) { scene.DestroyNode(root); return {}; }
    scene.SetHitTestVisible(background, false); scene.SetHitTestVisible(fill, false);
    scene.SetVisual(background, UIPanelVisual{desc.backgroundColor});
    scene.SetVisual(fill, UIPanelVisual{desc.fillColor});
    UILayoutParams full; full.mode = UILayoutMode::Absolute;
    full.anchor = {0,0,1,1}; full.sizeRule.width.mode = UISizeMode::Stretch;
    full.sizeRule.height.mode = UISizeMode::Stretch;
    scene.SetLayout(background, full);
    scene.SetStyleClass(background, desc.backgroundStyle); scene.SetStyleClass(fill, desc.fillStyle);
    UIWidgetRecord record; record.parentWidget = parent; record.rootNode = root;
    record.childHostNode = root; record.ownedNodes = {root, background, fill};
    record.kind = UIWidgetKind::ProgressBar; record.visible = desc.common.visible;
    record.enabled = desc.common.enabled; record.minimum = desc.minimum; record.maximum = desc.maximum;
    record.value = std::clamp(std::isfinite(desc.value) ? desc.value : desc.minimum,
        desc.minimum, desc.maximum); record.progressDirection = desc.direction;
    record.progressBackground = background; record.progressFill = fill;
    const UIWidgetHandle handle = RegisterOrRollback(std::move(record));
    if (UIWidgetRecord* stored = m_Registry.TryGet(handle)) (void)UpdateProgressGeometry(*stored);
    return handle;
}

float UIWidgetRuntime::Value(UIWidgetHandle widget) const {
    const UIWidgetRecord* record = m_Registry.TryGet(widget);
    return record && record->kind == UIWidgetKind::ProgressBar ? record->value : 0.0f;
}
bool UIWidgetRuntime::SetValue(UIWidgetHandle widget, float value) {
    UIWidgetRecord* record = m_Registry.TryGet(widget);
    if (!record || record->kind != UIWidgetKind::ProgressBar) return false;
    const float next = std::clamp(std::isfinite(value) ? value : record->minimum,
        record->minimum, record->maximum);
    if (record->value == next) return true;
    record->value = next; return UpdateProgressGeometry(*record);
}
bool UIWidgetRuntime::SetRange(UIWidgetHandle widget, float minimum, float maximum) {
    UIWidgetRecord* record = m_Registry.TryGet(widget);
    if (!record || record->kind != UIWidgetKind::ProgressBar || !std::isfinite(minimum) ||
        !std::isfinite(maximum) || minimum >= maximum) return false;
    record->minimum = minimum; record->maximum = maximum;
    record->value = std::clamp(record->value, minimum, maximum);
    return UpdateProgressGeometry(*record);
}
bool UIWidgetRuntime::SetProgressDirection(UIWidgetHandle widget, UIProgressDirection direction) {
    UIWidgetRecord* record = m_Registry.TryGet(widget);
    if (!record || record->kind != UIWidgetKind::ProgressBar) return false;
    record->progressDirection = direction; return UpdateProgressGeometry(*record);
}
bool UIWidgetRuntime::UpdateProgressGeometry(UIWidgetRecord& record) {
    if (!m_Runtime->Scene().TryGet(record.progressFill)) return false;
    const float normalized = std::clamp((record.value - record.minimum) /
        (record.maximum - record.minimum), 0.0f, 1.0f);
    UILayoutParams fill; fill.mode = UILayoutMode::Absolute;
    fill.sizeRule.width.mode = UISizeMode::Stretch;
    fill.sizeRule.height.mode = UISizeMode::Stretch;
    switch (record.progressDirection) {
    case UIProgressDirection::LeftToRight: fill.anchor = {0,0,normalized,1}; break;
    case UIProgressDirection::RightToLeft: fill.anchor = {1-normalized,0,1,1}; break;
    case UIProgressDirection::BottomToTop: fill.anchor = {0,1-normalized,1,1}; break;
    case UIProgressDirection::TopToBottom: fill.anchor = {0,0,1,normalized}; break;
    }
    return m_Runtime->Scene().SetLayout(record.progressFill, fill);
}

} // namespace Engine::UI2D
