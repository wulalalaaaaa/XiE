#include "UI2D/Widgets/Runtime/UIWidgetRuntime.h"

#include "Input/InputDiagnostics.h"
#include "Platform/IWindowSystem.h"
#include "Platform/TextInput/ITextClipboard.h"
#include "Platform/TextInput/ITextInputMethod.h"
#include "Renderer2D/ITextLayoutService.h"
#include "UI2D/Input/UIEventContext.h"
#include "UI2D/Runtime/UIWindowRuntime.h"
#include "UI2D/TextEditing/UITextEditingUtils.h"

#include <algorithm>

namespace Engine::UI2D {
namespace {

constexpr float kPadding = 8.0f;
constexpr float kCaretWidth = 1.5f;

void EmitTextBoxDiagnostic(UIWindowRuntime& runtime,
    UINodeHandle node, Engine::InputDiagnosticKind kind) {
    Engine::EmitInputDiagnostic({Engine::InputDiagnosticStage::TextBox,
        kind, runtime.Window(), node.index, node.generation});
}

UILayoutParams Fixed(float width, float height) {
    UILayoutParams result;
    result.mode = UILayoutMode::Absolute;
    result.sizeRule.width = {UISizeMode::Fixed, std::max(0.0f, width)};
    result.sizeRule.height = {UISizeMode::Fixed, std::max(0.0f, height)};
    return result;
}

bool HasModifier(
    Engine::InputModifiers modifiers, Engine::InputModifiers value) {
    return (static_cast<std::uint8_t>(modifiers) &
        static_cast<std::uint8_t>(value)) != 0;
}

std::string SelectedText(const UITextBoxState& state) {
    const auto start = Utf8ByteOffset(
        state.model.Text(), state.model.Selection().Start());
    const auto end = Utf8ByteOffset(
        state.model.Text(), state.model.Selection().End());
    if (!start || !end) return {};
    return state.model.Text().substr(*start, *end - *start);
}

void StopCaretBlink(
    UIWindowRuntime& runtime, UITextBoxState& state) {
    if (!state.caretBlink.IsValid()) return;
    (void)runtime.CancelAnimation(
        state.caretBlink, UIAnimationCancelMode::RestoreBase);
    state.caretBlink = {};
}

void StartCaretBlink(
    UIWindowRuntime& runtime, UITextBoxState& state) {
    StopCaretBlink(runtime, state);
    UIAnimationDesc blink;
    blink.node = state.caretNode;
    blink.property = UIAnimatedProperty::Opacity;
    blink.from = 1.0f;
    blink.to = 0.15f;
    blink.durationSeconds = 0.55;
    blink.infinite = true;
    blink.direction = UIAnimationDirection::Alternate;
    blink.easing = UIEasing::Linear;
    blink.fillMode = UIAnimationFillMode::Both;
    const auto started = runtime.StartAnimation(blink);
    if (started.success) state.caretBlink = started.handle;
}

void UpdateCandidateRect(
    UIWindowRuntime& runtime, const UIWidgetRecord& record) {
    const auto& services = runtime.Services();
    if (!record.textBox || !services.textInputMethod ||
        !services.windowSystem || !runtime.IsFocused(record.rootNode)) return;
    const UINodeRecord* root = runtime.Scene().TryGet(record.rootNode);
    if (!root) return;
    const float dpi = runtime.LastDpiScale();
    const Engine::Vec2F origin =
        services.windowSystem->GetWindowScreenPositionPhysical(
            runtime.Window());
    const float caretX = record.textBox->hitMap.PositionOf(
        record.textBox->model.CaretIndex());
    const Engine::RectF scene = root->layoutState.sceneRect;
    services.textInputMethod->SetCaretRect(runtime.Window(), {
        origin.x + (scene.x + kPadding + caretX -
            record.textBox->horizontalTextOffset) * dpi,
        origin.y + (scene.y + 5.0f) * dpi,
        kCaretWidth * dpi,
        std::max(1.0f, scene.height - 10.0f) * dpi});
}

void RefreshTextBox(
    UIWindowRuntime& runtime, UIWidgetRecord& record) {
    UITextBoxState& state = *record.textBox;
    UIScene& scene = runtime.Scene();
    const UINodeRecord* root = scene.TryGet(record.rootNode);
    const float width = root && root->layoutState.arrangedRect.width > 0
        ? root->layoutState.arrangedRect.width
        : state.viewportWidth + kPadding * 2.0f;
    const float height = root && root->layoutState.arrangedRect.height > 0
        ? root->layoutState.arrangedRect.height
        : std::max(30.0f, state.fontSize * 1.8f);
    state.viewportWidth = std::max(1.0f, width - kPadding * 2.0f);

    const bool placeholder =
        state.model.Text().empty() && !state.composition.active;
    const std::string committed = placeholder
        ? state.placeholder : state.model.Text();
    if (auto* layouts = runtime.Services().textLayouts) {
        if (const auto decoded = DecodeUtf8(committed)) {
            const auto layout = layouts->CreateLayout(
                state.font, *decoded, 4096.0f, state.fontSize);
            scene.SetVisual(state.textNode, UITextVisual{
                layout, placeholder ? state.placeholderColor : state.textColor,
                UITextHorizontalAlignment::Start,
                UITextVerticalAlignment::Center});
            if (!placeholder) {
                if (const auto* bitmap = layouts->GetBitmap(layout)) {
                    state.hitMap.stops.clear();
                    for (std::size_t i = 0;
                         i < bitmap->caretStops.size(); ++i) {
                        state.hitMap.stops.push_back({i,
                            bitmap->caretStops[i]});
                    }
                }
            } else {
                state.hitMap.stops = {{0, 0.0f}};
            }
        }
        if (state.composition.active) {
            if (const auto decoded =
                DecodeUtf8(state.composition.preeditText)) {
                scene.SetVisual(state.compositionNode, UITextVisual{
                    layouts->CreateLayout(
                        state.font, *decoded, 4096.0f, state.fontSize),
                    state.textColor,
                    UITextHorizontalAlignment::Start,
                    UITextVerticalAlignment::Center});
            }
        }
    }
    if (state.hitMap.stops.empty())
        state.hitMap.stops.push_back({0, 0.0f});

    const float caret = state.hitMap.PositionOf(state.model.CaretIndex());
    if (caret - state.horizontalTextOffset > state.viewportWidth)
        state.horizontalTextOffset = caret - state.viewportWidth;
    if (caret < state.horizontalTextOffset)
        state.horizontalTextOffset = caret;
    state.horizontalTextOffset = std::max(0.0f, state.horizontalTextOffset);

    UITransform textTransform;
    textTransform.position = {
        kPadding - state.horizontalTextOffset, 0.0f};
    textTransform.size = {4096.0f, height};
    scene.SetTransform(state.textNode, textTransform);
    scene.SetLayout(state.textNode, Fixed(4096.0f, height));
    scene.SetVisibility(state.textNode, true);

    const auto selection = state.model.Selection();
    const float selectionStart =
        state.hitMap.PositionOf(selection.Start());
    const float selectionEnd =
        state.hitMap.PositionOf(selection.End());
    UITransform selectionTransform;
    selectionTransform.position = {
        kPadding + selectionStart - state.horizontalTextOffset, 4.0f};
    selectionTransform.size = {
        std::max(0.0f, selectionEnd - selectionStart),
        std::max(1.0f, height - 8.0f)};
    scene.SetTransform(state.selectionNode, selectionTransform);
    scene.SetLayout(state.selectionNode, Fixed(
        selectionTransform.size.x, selectionTransform.size.y));
    scene.SetVisibility(state.selectionNode,
        runtime.IsFocused(record.rootNode) && !selection.Empty());

    UITransform caretTransform;
    caretTransform.position = {
        kPadding + caret - state.horizontalTextOffset, 5.0f};
    caretTransform.size = {kCaretWidth, std::max(1.0f, height - 10.0f)};
    scene.SetTransform(state.caretNode, caretTransform);
    scene.SetLayout(state.caretNode, Fixed(
        caretTransform.size.x, caretTransform.size.y));
    scene.SetVisibility(state.caretNode,
        runtime.IsFocused(record.rootNode) && selection.Empty());

    const float compositionX = caret;
    UITransform compositionTransform;
    compositionTransform.position = {
        kPadding + compositionX - state.horizontalTextOffset, 0.0f};
    compositionTransform.size = {4096.0f, height};
    scene.SetTransform(state.compositionNode, compositionTransform);
    scene.SetLayout(state.compositionNode, Fixed(4096.0f, height));
    scene.SetVisibility(
        state.compositionNode, state.composition.active);
    UpdateCandidateRect(runtime, record);
}

void NotifyTextChanged(UITextBoxState& state) {
    if (state.onTextChanged) state.onTextChanged(state.model.Text());
}

} // namespace

UIWidgetHandle UITextBoxWidget::Create(
    UIWidgetContext& context, const UITextBoxWidgetDesc& desc,
    UIWidgetHandle parent) {
    return context.windowRuntime.Widgets().CreateTextBox(desc, parent);
}

UIWidgetHandle UIWidgetRuntime::CreateTextBox(
    const UITextBoxWidgetDesc& desc, UIWidgetHandle parent) {
    UIScene& scene = m_Runtime->Scene();
    const UINodeHandle root =
        scene.CreateNode("TextBoxWidget", ParentNode(parent));
    UIWidgetCommonProperties common = desc.common;
    if (desc.rootStyle.IsValid()) common.styleClass = desc.rootStyle;
    if (!root.IsValid() || !ApplyCommon(root, common)) return {};
    scene.SetVisual(root, UIPanelVisual{
        {0.10f, 0.11f, 0.14f, 1.0f}, 4.0f, true});
    scene.SetClipChildren(root, true);

    const UINodeHandle selection =
        scene.CreateNode("TextBox.Selection", root);
    const UINodeHandle text = scene.CreateNode("TextBox.Text", root);
    const UINodeHandle composition =
        scene.CreateNode("TextBox.Composition", root);
    const UINodeHandle caret = scene.CreateNode("TextBox.Caret", root);
    if (!selection.IsValid() || !text.IsValid() ||
        !composition.IsValid() || !caret.IsValid()) {
        scene.DestroyNode(root);
        return {};
    }
    scene.SetVisual(selection, UIPanelVisual{
        desc.selectionColor, 0.0f, false});
    scene.SetVisual(caret, UIPanelVisual{
        desc.caretColor, 0.0f, false});
    scene.SetHitTestVisible(selection, false);
    scene.SetHitTestVisible(text, false);
    scene.SetHitTestVisible(composition, false);
    scene.SetHitTestVisible(caret, false);
    if (desc.selectionStyle.IsValid())
        scene.SetStyleClass(selection, desc.selectionStyle);
    if (desc.placeholderStyle.IsValid())
        scene.SetStyleClass(text, desc.placeholderStyle);
    if (desc.caretStyle.IsValid())
        scene.SetStyleClass(caret, desc.caretStyle);

    UIWidgetRecord record;
    record.parentWidget = parent;
    record.rootNode = root;
    record.childHostNode = root;
    record.ownedNodes = {root, selection, text, composition, caret};
    record.kind = UIWidgetKind::TextBox;
    record.visible = desc.common.visible;
    record.enabled = desc.common.enabled;
    record.textBox =
        std::make_unique<UITextBoxState>(desc.maxCodepoints);
    auto& state = *record.textBox;
    if (!state.model.SetText(desc.initialText)) {
        scene.DestroyNode(root);
        return {};
    }
    state.placeholder = desc.placeholder;
    state.readOnly = desc.readOnly;
    state.selectAllOnFocus = desc.selectAllOnFocus;
    state.viewportWidth = std::max(
        1.0f, desc.common.transform.size.x - kPadding * 2.0f);
    state.font = desc.font;
    state.fontSize = desc.fontSize;
    state.textColor = desc.textColor;
    state.placeholderColor = desc.placeholderColor;
    state.selectionColor = desc.selectionColor;
    state.caretColor = desc.caretColor;
    state.placeholderStyle = desc.placeholderStyle;
    state.selectionStyle = desc.selectionStyle;
    state.caretStyle = desc.caretStyle;
    state.selectionNode = selection;
    state.textNode = text;
    state.compositionNode = composition;
    state.caretNode = caret;
    state.onTextChanged = desc.onTextChanged;
    state.onSubmitted = desc.onSubmitted;
    state.onEditingCanceled = desc.onEditingCanceled;
    state.onFocusEditingStarted = desc.onFocusEditingStarted;
    state.onFocusEditingEnded = desc.onFocusEditingEnded;

    const UIWidgetHandle handle = RegisterOrRollback(std::move(record));
    if (!handle.IsValid()) return {};
    m_TextBoxes.push_back(handle);

    auto connect = [&](UIEventType type, UIEventCallback callback) {
        return Connect(handle, root, type, UIEventPhaseMask::Target,
            std::move(callback));
    };
    connect(UIEventType::TextInput,
        [this, handle](UIEventContext& context) {
            UIWidgetRecord* record = m_Registry.TryGet(handle);
            if (!record || !record->textBox ||
                record->textBox->readOnly) return;
            EmitTextBoxDiagnostic(*m_Runtime, record->rootNode,
                Engine::InputDiagnosticKind::TextInput);
            const char32_t scalar = context.Event().codepoint;
            if (scalar == U'\r' || scalar == U'\n' || scalar == U'\0')
                return;
            const std::string utf8 = EncodeUtf8(
                std::u32string_view(&scalar, 1));
            if (record->textBox->model.InsertText(utf8)) {
                NotifyTextChanged(*record->textBox);
                RefreshTextBox(*m_Runtime, *record);
                context.MarkHandled();
            }
        });
    connect(UIEventType::KeyDown,
        [this, handle](UIEventContext& context) {
            UIWidgetRecord* record = m_Registry.TryGet(handle);
            if (!record || !record->textBox) return;
            EmitTextBoxDiagnostic(*m_Runtime, record->rootNode,
                Engine::InputDiagnosticKind::KeyDown);
            auto& state = *record->textBox;
            const auto key = context.Event().key;
            const bool shift = HasModifier(
                context.Event().modifiers, Engine::InputModifiers::Shift);
            const bool control = HasModifier(
                context.Event().modifiers, Engine::InputModifiers::Control);
            bool changed = false;
            bool textChanged = false;
            if (key == Engine::InputKeyCode::Left)
                changed = state.model.MoveLeft(shift);
            else if (key == Engine::InputKeyCode::Right)
                changed = state.model.MoveRight(shift);
            else if (key == Engine::InputKeyCode::Home)
                changed = state.model.MoveHome(shift);
            else if (key == Engine::InputKeyCode::End)
                changed = state.model.MoveEnd(shift);
            else if (control && key == Engine::InputKeyCode::A)
                changed = state.model.SelectAll();
            else if (control && key == Engine::InputKeyCode::C) {
                if (auto* clipboard =
                    m_Runtime->Services().textClipboard) {
                    (void)clipboard->WriteText(SelectedText(state));
                    changed = true;
                }
            } else if (!state.readOnly && control &&
                key == Engine::InputKeyCode::X) {
                if (auto* clipboard =
                    m_Runtime->Services().textClipboard) {
                    (void)clipboard->WriteText(SelectedText(state));
                    textChanged = state.model.ReplaceSelection({});
                    changed = textChanged;
                }
            } else if (!state.readOnly && control &&
                key == Engine::InputKeyCode::V) {
                if (auto* clipboard =
                    m_Runtime->Services().textClipboard) {
                    auto read = clipboard->ReadText();
                    textChanged = read.success &&
                        state.model.ReplaceSelection(read.text);
                    changed = textChanged;
                }
            } else if (!state.readOnly && control &&
                key == Engine::InputKeyCode::Z) {
                textChanged = state.model.Undo();
                changed = textChanged;
            } else if (!state.readOnly && control &&
                key == Engine::InputKeyCode::Y) {
                textChanged = state.model.Redo();
                changed = textChanged;
            } else if (!state.readOnly &&
                key == Engine::InputKeyCode::Backspace) {
                textChanged = state.model.DeleteBackward();
                changed = textChanged;
            } else if (!state.readOnly &&
                key == Engine::InputKeyCode::Delete) {
                textChanged = state.model.DeleteForward();
                changed = textChanged;
            } else if (key == Engine::InputKeyCode::Enter) {
                if (state.onSubmitted)
                    state.onSubmitted(state.model.Text());
                changed = true;
            } else if (key == Engine::InputKeyCode::Escape) {
                if (state.composition.active) {
                    if (auto* inputMethod =
                        m_Runtime->Services().textInputMethod) {
                        inputMethod->CancelComposition(
                            m_Runtime->Window());
                    }
                    state.composition = {};
                    changed = true;
                } else if (state.onEditingCanceled) {
                    state.onEditingCanceled();
                    changed = true;
                }
            }
            if (textChanged) NotifyTextChanged(state);
            if (changed) {
                RefreshTextBox(*m_Runtime, *record);
                context.MarkHandled();
            }
        });
    connect(UIEventType::TextCompositionStart,
        [this, handle](UIEventContext&) {
            if (auto* record = m_Registry.TryGet(handle);
                record && record->textBox && !record->textBox->readOnly) {
                EmitTextBoxDiagnostic(*m_Runtime, record->rootNode,
                    Engine::InputDiagnosticKind::CompositionStart);
                record->textBox->composition = {};
                record->textBox->composition.active = true;
                RefreshTextBox(*m_Runtime, *record);
            }
        });
    connect(UIEventType::TextCompositionUpdate,
        [this, handle](UIEventContext& context) {
            auto* record = m_Registry.TryGet(handle);
            if (!record || !record->textBox ||
                record->textBox->readOnly ||
                !IsValidUtf8(context.Event().text)) return;
            EmitTextBoxDiagnostic(*m_Runtime, record->rootNode,
                Engine::InputDiagnosticKind::CompositionUpdate);
            auto& composition = record->textBox->composition;
            composition.active = true;
            composition.preeditText = context.Event().text;
            composition.caret = context.Event().textCaret;
            if (context.Event().textCompositionSelection) {
                const auto value =
                    *context.Event().textCompositionSelection;
                composition.selection =
                    UITextSelection{value.anchor, value.active};
            } else composition.selection.reset();
            RefreshTextBox(*m_Runtime, *record);
            context.MarkHandled();
        });
    connect(UIEventType::TextCompositionCommit,
        [this, handle](UIEventContext& context) {
            auto* record = m_Registry.TryGet(handle);
            if (!record || !record->textBox ||
                record->textBox->readOnly) return;
            EmitTextBoxDiagnostic(*m_Runtime, record->rootNode,
                Engine::InputDiagnosticKind::CompositionCommit);
            auto& state = *record->textBox;
            state.composition = {};
            if (state.model.ReplaceSelection(context.Event().text)) {
                NotifyTextChanged(state);
                RefreshTextBox(*m_Runtime, *record);
            }
            context.MarkHandled();
        });
    connect(UIEventType::TextCompositionEnd,
        [this, handle](UIEventContext&) {
            if (auto* record = m_Registry.TryGet(handle);
                record && record->textBox) {
                EmitTextBoxDiagnostic(*m_Runtime, record->rootNode,
                    Engine::InputDiagnosticKind::CompositionEnd);
                record->textBox->composition = {};
                RefreshTextBox(*m_Runtime, *record);
            }
        });
    connect(UIEventType::FocusGained,
        [this, handle](UIEventContext&) {
            auto* record = m_Registry.TryGet(handle);
            if (!record || !record->textBox) return;
            EmitTextBoxDiagnostic(*m_Runtime, record->rootNode,
                Engine::InputDiagnosticKind::FocusGained);
            if (record->textBox->selectAllOnFocus)
                record->textBox->model.SelectAll();
            if (auto* inputMethod =
                m_Runtime->Services().textInputMethod)
                inputMethod->Activate(m_Runtime->Window());
            if (record->textBox->onFocusEditingStarted)
                record->textBox->onFocusEditingStarted();
            RefreshTextBox(*m_Runtime, *record);
            StartCaretBlink(*m_Runtime, *record->textBox);
        });
    connect(UIEventType::FocusLost,
        [this, handle](UIEventContext&) {
            auto* record = m_Registry.TryGet(handle);
            if (!record || !record->textBox) return;
            EmitTextBoxDiagnostic(*m_Runtime, record->rootNode,
                Engine::InputDiagnosticKind::FocusLost);
            record->textBox->composition = {};
            record->textBox->selecting = false;
            if (auto* inputMethod =
                m_Runtime->Services().textInputMethod)
                inputMethod->Deactivate(m_Runtime->Window());
            if (record->textBox->onFocusEditingEnded)
                record->textBox->onFocusEditingEnded();
            StopCaretBlink(*m_Runtime, *record->textBox);
            RefreshTextBox(*m_Runtime, *record);
        });
    connect(UIEventType::PointerDown,
        [this, handle](UIEventContext& context) {
            auto* record = m_Registry.TryGet(handle);
            if (!record || !record->textBox ||
                context.Event().button !=
                    Engine::PointerButton::Primary) return;
            EmitTextBoxDiagnostic(*m_Runtime, record->rootNode,
                Engine::InputDiagnosticKind::PointerDown);
            auto& state = *record->textBox;
            const std::size_t index = state.hitMap.HitTest(
                context.Event().localPosition.x - kPadding +
                state.horizontalTextOffset);
            if (HasModifier(context.Event().modifiers,
                Engine::InputModifiers::Shift)) {
                auto selection = state.model.Selection();
                selection.active = index;
                state.model.SetSelection(selection);
            } else state.model.SetCaret(index);
            state.selecting = true;
            state.selectionPointer = context.Event().pointerId;
            context.RequestPointerCapture(
                context.Event().pointerId, record->rootNode);
            context.RequestFocus(record->rootNode);
            context.MarkHandled();
            RefreshTextBox(*m_Runtime, *record);
        });
    connect(UIEventType::PointerMove,
        [this, handle](UIEventContext& context) {
            auto* record = m_Registry.TryGet(handle);
            if (!record || !record->textBox ||
                !record->textBox->selecting ||
                record->textBox->selectionPointer !=
                    context.Event().pointerId) return;
            auto selection = record->textBox->model.Selection();
            selection.active = record->textBox->hitMap.HitTest(
                context.Event().localPosition.x - kPadding +
                record->textBox->horizontalTextOffset);
            record->textBox->model.SetSelection(selection);
            context.MarkHandled();
            RefreshTextBox(*m_Runtime, *record);
        });
    connect(UIEventType::PointerUp,
        [this, handle](UIEventContext& context) {
            auto* record = m_Registry.TryGet(handle);
            if (!record || !record->textBox ||
                !record->textBox->selecting ||
                record->textBox->selectionPointer !=
                    context.Event().pointerId) return;
            record->textBox->selecting = false;
            context.ReleasePointerCapture(context.Event().pointerId);
            context.MarkHandled();
        });
    if (auto* created = m_Registry.TryGet(handle))
        RefreshTextBox(*m_Runtime, *created);
    return handle;
}

bool UIWidgetRuntime::SetText(
    UIWidgetHandle widget, std::string_view text) {
    auto* record = m_Registry.TryGet(widget);
    if (!record || !record->textBox) return false;
    const bool result = record->textBox->model.SetText(std::string(text));
    if (result) RefreshTextBox(*m_Runtime, *record);
    return result;
}

std::string UIWidgetRuntime::GetText(UIWidgetHandle widget) const {
    const auto* record = m_Registry.TryGet(widget);
    return record && record->textBox
        ? record->textBox->model.Text() : std::string{};
}

bool UIWidgetRuntime::SetReadOnly(
    UIWidgetHandle widget, bool readOnly) {
    auto* record = m_Registry.TryGet(widget);
    if (!record || !record->textBox) return false;
    record->textBox->readOnly = readOnly;
    return true;
}

bool UIWidgetRuntime::SelectAll(UIWidgetHandle widget) {
    auto* record = m_Registry.TryGet(widget);
    if (!record || !record->textBox) return false;
    const bool result = record->textBox->model.SelectAll();
    RefreshTextBox(*m_Runtime, *record);
    return result;
}

bool UIWidgetRuntime::SetCaret(
    UIWidgetHandle widget, std::size_t index) {
    auto* record = m_Registry.TryGet(widget);
    if (!record || !record->textBox ||
        !record->textBox->model.SetCaret(index)) return false;
    RefreshTextBox(*m_Runtime, *record);
    return true;
}

UITextSelection UIWidgetRuntime::GetSelection(
    UIWidgetHandle widget) const {
    const auto* record = m_Registry.TryGet(widget);
    return record && record->textBox
        ? record->textBox->model.Selection() : UITextSelection{};
}

} // namespace Engine::UI2D
