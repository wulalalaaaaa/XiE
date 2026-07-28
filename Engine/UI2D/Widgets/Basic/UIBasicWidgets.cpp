#include "UI2D/Widgets/Runtime/UIWidgetRuntime.h"

#include "UI2D/Runtime/UIWindowRuntime.h"

#include <algorithm>

namespace Engine::UI2D {

UIWidgetHandle UIWidgetRuntime::CreatePanel(const UIPanelWidgetDesc& desc, UIWidgetHandle parent) {
    UIScene& scene = m_Runtime->Scene();
    const UINodeHandle root = scene.CreateNode("PanelWidget", ParentNode(parent));
    if (!root.IsValid() || !ApplyCommon(root, desc.common)) return {};
    if (desc.nineSliceTexture.IsValid()) {
        UINineSliceVisual visual; visual.texture = desc.nineSliceTexture;
        visual.borders = desc.nineSliceBorders; visual.sourceBorders = desc.nineSliceBorders;
        visual.destinationBorders = desc.nineSliceBorders; visual.tint = desc.backgroundColor;
        scene.SetVisual(root, visual);
    } else {
        scene.SetVisual(root, UIPanelVisual{desc.backgroundColor, std::max(0.0f, desc.cornerRadius),
            desc.cornerRadius > 0.0f});
    }
    scene.SetClipChildren(root, desc.clipChildren);
    UIWidgetRecord record; record.parentWidget = parent; record.rootNode = root;
    record.childHostNode = root; record.ownedNodes = {root}; record.kind = UIWidgetKind::Panel;
    record.visible = desc.common.visible; record.enabled = desc.common.enabled;
    return RegisterOrRollback(std::move(record));
}

UIWidgetHandle UIWidgetRuntime::CreateImage(const UIImageWidgetDesc& desc, UIWidgetHandle parent) {
    UIScene& scene = m_Runtime->Scene();
    const UINodeHandle root = scene.CreateNode("ImageWidget", ParentNode(parent));
    if (!root.IsValid() || !ApplyCommon(root, desc.common)) return {};
    UIImageVisual visual; visual.texture = desc.texture; visual.fit = desc.fit;
    visual.preserveAspectRatio = desc.fit != UIImageFit::Stretch; visual.tint = desc.tint;
    scene.SetVisual(root, visual);
    UIWidgetRecord record; record.parentWidget = parent; record.rootNode = root;
    record.childHostNode = root; record.ownedNodes = {root}; record.kind = UIWidgetKind::Image;
    record.visible = desc.common.visible; record.enabled = desc.common.enabled;
    return RegisterOrRollback(std::move(record));
}

UIWidgetHandle UIWidgetRuntime::CreateText(const UITextWidgetDesc& desc, UIWidgetHandle parent) {
    UIScene& scene = m_Runtime->Scene();
    const UINodeHandle root = scene.CreateNode("TextWidget", ParentNode(parent));
    if (!root.IsValid() || !ApplyCommon(root, desc.common)) return {};
    scene.SetVisual(root, UITextVisual{desc.textLayout, desc.color,
        desc.horizontalAlignment, desc.verticalAlignment});
    UIWidgetRecord record; record.parentWidget = parent; record.rootNode = root;
    record.childHostNode = root; record.ownedNodes = {root}; record.kind = UIWidgetKind::Text;
    record.visible = desc.common.visible; record.enabled = desc.common.enabled;
    return RegisterOrRollback(std::move(record));
}

bool UIWidgetRuntime::SetTexture(UIWidgetHandle widget, Engine::TextureHandle texture) {
    UIWidgetRecord* record = m_Registry.TryGet(widget);
    if (!record || record->kind != UIWidgetKind::Image) return false;
    auto* visual = std::get_if<UIImageVisual>(&m_Runtime->Scene().TryGet(record->rootNode)->visual);
    if (!visual) return false; UIImageVisual next = *visual; next.texture = texture;
    return m_Runtime->Scene().SetVisual(record->rootNode, next);
}
bool UIWidgetRuntime::SetImageFit(UIWidgetHandle widget, UIImageFit fit) {
    UIWidgetRecord* record = m_Registry.TryGet(widget);
    if (!record || record->kind != UIWidgetKind::Image) return false;
    auto* visual = std::get_if<UIImageVisual>(&m_Runtime->Scene().TryGet(record->rootNode)->visual);
    if (!visual) return false; UIImageVisual next = *visual; next.fit = fit;
    next.preserveAspectRatio = fit != UIImageFit::Stretch;
    return m_Runtime->Scene().SetVisual(record->rootNode, next);
}
bool UIWidgetRuntime::SetTint(UIWidgetHandle widget, Engine::Color4f tint) {
    UIWidgetRecord* record = m_Registry.TryGet(widget);
    if (!record || record->kind != UIWidgetKind::Image) return false;
    auto* visual = std::get_if<UIImageVisual>(&m_Runtime->Scene().TryGet(record->rootNode)->visual);
    if (!visual) return false; UIImageVisual next = *visual; next.tint = tint;
    return m_Runtime->Scene().SetVisual(record->rootNode, next);
}
bool UIWidgetRuntime::SetTextLayout(UIWidgetHandle widget, Engine::TextLayoutHandle layout) {
    UIWidgetRecord* record = m_Registry.TryGet(widget);
    if (!record || record->kind != UIWidgetKind::Text) return false;
    auto* visual = std::get_if<UITextVisual>(&m_Runtime->Scene().TryGet(record->rootNode)->visual);
    if (!visual) return false; UITextVisual next = *visual; next.layout = layout;
    return m_Runtime->Scene().SetVisual(record->rootNode, next);
}
bool UIWidgetRuntime::SetTextColor(UIWidgetHandle widget, Engine::Color4f color) {
    UIWidgetRecord* record = m_Registry.TryGet(widget);
    if (!record || record->kind != UIWidgetKind::Text) return false;
    auto* visual = std::get_if<UITextVisual>(&m_Runtime->Scene().TryGet(record->rootNode)->visual);
    if (!visual) return false; UITextVisual next = *visual; next.color = color;
    return m_Runtime->Scene().SetVisual(record->rootNode, next);
}

} // namespace Engine::UI2D
