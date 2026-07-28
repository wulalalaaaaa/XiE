#include "UI2D/Widgets/Runtime/UIWidgetRuntime.h"

#include "UI2D/Input/UIEventContext.h"
#include "UI2D/Runtime/UIWindowRuntime.h"

#include <algorithm>

namespace Engine::UI2D {

namespace {
void SetPanelVisual(UIScene& scene, UINodeHandle node, const UIPanelWidgetDesc& desc) {
    if (desc.nineSliceTexture.IsValid()) {
        UINineSliceVisual visual; visual.texture = desc.nineSliceTexture;
        visual.borders = desc.nineSliceBorders; visual.sourceBorders = desc.nineSliceBorders;
        visual.destinationBorders = desc.nineSliceBorders; visual.tint = desc.backgroundColor;
        scene.SetVisual(node, visual);
    } else {
        scene.SetVisual(node, UIPanelVisual{desc.backgroundColor,
            std::max(0.0f, desc.cornerRadius), desc.cornerRadius > 0.0f});
    }
}
}

UIWidgetHandle UIWidgetRuntime::CreateButton(const UIButtonWidgetDesc& desc, UIWidgetHandle parent) {
    UIScene& scene = m_Runtime->Scene();
    const UINodeHandle root = scene.CreateNode("ButtonWidget", ParentNode(parent));
    if (!root.IsValid() || !ApplyCommon(root, desc.panel.common)) return {};
    SetPanelVisual(scene, root, desc.panel);
    scene.SetClipChildren(root, desc.panel.clipChildren);
    UIWidgetRecord record; record.parentWidget = parent; record.rootNode = root;
    record.childHostNode = root; record.ownedNodes = {root}; record.kind = UIWidgetKind::Button;
    record.visible = desc.panel.common.visible; record.enabled = desc.panel.common.enabled;
    record.buttonBehavior = desc.behavior;
    const UIWidgetHandle handle = RegisterOrRollback(std::move(record));
    if (handle.IsValid()) InstallButtonBehavior(handle);
    return handle;
}

UIWidgetHandle UIWidgetRuntime::CreateToggleButton(
    const UIToggleButtonWidgetDesc& desc, UIWidgetHandle parent) {
    UIScene& scene = m_Runtime->Scene();
    const UINodeHandle root = scene.CreateNode("ToggleButtonWidget", ParentNode(parent));
    if (!root.IsValid() || !ApplyCommon(root, desc.button.panel.common)) return {};
    SetPanelVisual(scene, root, desc.button.panel);
    scene.SetClipChildren(root, desc.button.panel.clipChildren);
    UIWidgetRecord record; record.parentWidget = parent; record.rootNode = root;
    record.childHostNode = root; record.ownedNodes = {root}; record.kind = UIWidgetKind::ToggleButton;
    record.visible = desc.button.panel.common.visible; record.enabled = desc.button.panel.common.enabled;
    record.buttonBehavior = desc.button.behavior; record.checked = false;
    record.allowUncheck = desc.allowUncheck;
    const UIWidgetHandle handle = RegisterOrRollback(std::move(record));
    if (handle.IsValid()) {
        InstallButtonBehavior(handle);
        (void)SetChecked(handle, desc.checked, false);
    }
    return handle;
}

UIWidgetConnectionHandle UIWidgetRuntime::OnActivated(
    UIWidgetHandle widget, UIButtonActivatedCallback callback) {
    const UIWidgetRecord* record = m_Registry.TryGet(widget);
    if (!record || (record->kind != UIWidgetKind::Button && record->kind != UIWidgetKind::ToggleButton)) return {};
    return m_Registry.OnActivated(widget, std::move(callback));
}
bool UIWidgetRuntime::IsUserEnabled(const UIWidgetRecord& record) const {
    const UINodeRecord* node = m_Runtime->Scene().TryGet(record.rootNode);
    return record.enabled && node && node->enabled &&
        (!node->layoutState.arrangeValid || node->layoutState.effectiveEnabled);
}
bool UIWidgetRuntime::Activate(UIWidgetHandle widget, UIButtonActivatedEvent::Source source) {
    UIWidgetRecord* record = m_Registry.TryGet(widget);
    if (!record || (record->kind != UIWidgetKind::Button && record->kind != UIWidgetKind::ToggleButton) ||
        record->activationInProgress || record->invalidationQueued || !IsUserEnabled(*record)) return false;
    record->activationInProgress = true;
    if (record->kind == UIWidgetKind::ToggleButton && (!record->checked || record->allowUncheck))
        (void)SetChecked(widget, !record->checked, true);
    m_Registry.DispatchActivated({widget, source});
    if (UIWidgetRecord* current = m_Registry.TryGet(widget)) current->activationInProgress = false;
    return true;
}

void UIWidgetRuntime::InstallButtonBehavior(UIWidgetHandle widget) {
    UIWidgetRecord* record = m_Registry.TryGet(widget);
    if (!record) return;
    const UINodeHandle node = record->rootNode;
    if (!record->buttonBehavior.focusOnPointerDown) {
        (void)Connect(widget, node, UIEventType::PointerDown,
            UIEventPhaseMask::Bubble,
            [this](UIEventContext& context) {
                const UINodeHandle focused = m_Runtime->FocusedNode();
                if (focused.IsValid()) (void)context.RequestFocus(focused);
                else context.ClearFocus();
            });
    }
    // Content widgets may be the hit target, so activation also listens while bubbling.
    (void)Connect(widget, node, UIEventType::Click,
        UIEventPhaseMask::Bubble,
        [this, widget](UIEventContext&) {
            const UIWidgetRecord* current = m_Registry.TryGet(widget);
            if (current && current->buttonBehavior.activateOnPointerClick)
                (void)Activate(widget, UIButtonActivatedEvent::Source::Pointer);
        });
    (void)Connect(widget, node, UIEventType::KeyDown, UIEventPhaseMask::Target,
        [this, widget](UIEventContext& context) {
            UIWidgetRecord* current = m_Registry.TryGet(widget);
            if (!current || context.Event().repeat) return;
            if (context.Event().key == Engine::InputKeyCode::Enter && current->buttonBehavior.activateOnEnter)
                (void)Activate(widget, UIButtonActivatedEvent::Source::KeyboardEnter);
            if (context.Event().key == Engine::InputKeyCode::Space && current->buttonBehavior.activateOnSpace &&
                !current->spacePressed) {
                current->spacePressed = true;
                UIInteractionStateMask states = m_Runtime->Scene().GetExplicitInteractionStates(current->rootNode);
                states |= UIInteractionState::Pressed;
                m_Runtime->Scene().SetExplicitInteractionStates(current->rootNode, states);
            }
        });
    (void)Connect(widget, node, UIEventType::KeyUp, UIEventPhaseMask::Target,
        [this, widget](UIEventContext& context) {
            UIWidgetRecord* current = m_Registry.TryGet(widget);
            if (!current || context.Event().key != Engine::InputKeyCode::Space || !current->spacePressed) return;
            current->spacePressed = false;
            UIInteractionStateMask states = m_Runtime->Scene().GetExplicitInteractionStates(current->rootNode);
            states &= ~ToMask(UIInteractionState::Pressed);
            m_Runtime->Scene().SetExplicitInteractionStates(current->rootNode, states);
            (void)Activate(widget, UIButtonActivatedEvent::Source::KeyboardSpace);
        });
    (void)Connect(widget, node, UIEventType::FocusLost, UIEventPhaseMask::Target,
        [this, widget](UIEventContext&) {
            UIWidgetRecord* current = m_Registry.TryGet(widget);
            if (!current || !current->spacePressed) return;
            current->spacePressed = false;
            UIInteractionStateMask states = m_Runtime->Scene().GetExplicitInteractionStates(current->rootNode);
            states &= ~ToMask(UIInteractionState::Pressed);
            m_Runtime->Scene().SetExplicitInteractionStates(current->rootNode, states);
        });
}

bool UIWidgetRuntime::IsChecked(UIWidgetHandle widget) const {
    const UIWidgetRecord* record = m_Registry.TryGet(widget);
    return record && record->kind == UIWidgetKind::ToggleButton && record->checked;
}
bool UIWidgetRuntime::SetChecked(UIWidgetHandle widget, bool checked, bool emitEvent) {
    UIWidgetRecord* record = m_Registry.TryGet(widget);
    if (!record || record->kind != UIWidgetKind::ToggleButton) return false;
    if (record->checked == checked) return true;
    record->checked = checked;
    UIInteractionStateMask states = m_Runtime->Scene().GetExplicitInteractionStates(record->rootNode);
    if (checked) states |= UIInteractionState::Checked;
    else states &= ~ToMask(UIInteractionState::Checked);
    m_Runtime->Scene().SetExplicitInteractionStates(record->rootNode, states);
    if (emitEvent) m_Registry.DispatchCheckedChanged(widget, checked);
    return true;
}
UIWidgetConnectionHandle UIWidgetRuntime::OnCheckedChanged(
    UIWidgetHandle widget, UIToggleCheckedChangedCallback callback) {
    const UIWidgetRecord* record = m_Registry.TryGet(widget);
    if (!record || record->kind != UIWidgetKind::ToggleButton) return {};
    return m_Registry.OnCheckedChanged(widget, std::move(callback));
}

} // namespace Engine::UI2D
