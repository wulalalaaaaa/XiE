#include "Platform/TextInput/ITextClipboard.h"
#include "Platform/TextInput/ITextInputMethod.h"
#include "Renderer2D/TextLayoutService.h"
#include "UI2D/HitTest/BasicUIHitTester.h"
#include "UI2D/Input/BasicUIInputRouter.h"
#include "UI2D/Layout/BasicLayoutEngine.h"
#include "UI2D/Rendering/BasicUIRenderBuilder.h"
#include "UI2D/Runtime/UIWindowRuntime.h"
#include "UI2D/Widgets/Text/UITextBoxWidget.h"

#include <iostream>

namespace {
using namespace Engine;
using namespace Engine::UI2D;
int failures = 0;
void Expect(bool value, const char* message) {
    std::cout << (value ? "[PASS] " : "[FAIL] ") << message << '\n';
    if (!value) ++failures;
}
UILayoutParams Fixed(float w, float h) {
    UILayoutParams p; p.sizeRule.width = {UISizeMode::Fixed, w};
    p.sizeRule.height = {UISizeMode::Fixed, h}; return p;
}
struct Clipboard final : ITextClipboard {
    std::string value;
    bool HasText() const override { return !value.empty(); }
    TextClipboardReadResult ReadText() override {
        return {true, value, {}};
    }
    TextClipboardWriteResult WriteText(std::string_view text) override {
        value = text; return {true, {}};
    }
};
struct Ime final : ITextInputMethod {
    int activates = 0, deactivates = 0, cancels = 0;
    void Activate(WindowHandle) override { ++activates; }
    void Deactivate(WindowHandle) override { ++deactivates; }
    void SetCaretRect(WindowHandle, const RectF&) override {}
    void CancelComposition(WindowHandle) override { ++cancels; }
};
struct Fixture {
    WindowHandle window{901, 1};
    BasicTextLayoutService textLayouts;
    BasicLayoutEngine layout{nullptr, &textLayouts};
    BasicUIHitTester hit;
    BasicUIInputRouter input;
    BasicUIRenderBuilder render{nullptr, &textLayouts};
    Clipboard clipboard;
    Ime ime;
    UIWindowRuntime runtime{window,
        {layout, hit, input, render, &textLayouts, &clipboard, &ime},
        MakeDefaultResolvedUITheme()};
    UIWidgetHandle box;
    int changes = 0, submits = 0, cancels = 0;
    Fixture() {
        UITextBoxWidgetDesc desc;
        desc.common.layout = Fixed(220, 36);
        desc.common.transform.position = {20, 20};
        desc.common.transform.size = {220, 36};
        desc.placeholder = "Search";
        desc.onTextChanged = [&](std::string_view) { ++changes; };
        desc.onSubmitted = [&](std::string_view) { ++submits; };
        desc.onEditingCanceled = [&] { ++cancels; };
        box = runtime.Widgets().CreateTextBox(desc);
        runtime.EnqueueInput(WindowFocusEvent{window, true, 0.0});
        Update();
        runtime.RequestFocus(
            runtime.Widgets().Registry().TryGet(box)->rootNode);
        Update();
    }
    void Update() { runtime.Update(0.0, {400, 180}, 1.0f); }
    void Key(InputKeyCode key, InputModifiers modifiers = {}) {
        runtime.EnqueueInput(KeyEvent{
            window, key, true, false, modifiers, 0.0});
        Update();
    }
    void Text(char32_t scalar) {
        runtime.EnqueueInput(TextInputEvent{window, scalar, 0.0});
        Update();
    }
};
}

int main() {
    const std::string emoji = "\xF0\x9F\x98\x80";
    const std::string chineseA = "A\xE4\xB8\xAD" + emoji;
    const std::string aEmoji = "A" + emoji;
    const std::string you = "\xE4\xBD\xA0";
    Fixture f;
    Expect(f.box.IsValid(), "TextBox creates a mounted widget");
    Expect(f.ime.activates == 1, "focus activates the input method");
    Expect(f.runtime.QueryFrameActivity().hasActiveAnimation,
        "focused caret blink uses UI Animator activity");
    f.Text(U'A'); f.Text(U'\u4E2D'); f.Text(U'\U0001F600');
    Expect(f.runtime.Widgets().GetText(f.box) == chineseA &&
        f.changes == 3, "committed Unicode input changes text once");
    f.Key(InputKeyCode::Left);
    f.Key(InputKeyCode::Backspace);
    Expect(f.runtime.Widgets().GetText(f.box) == aEmoji,
        "backspace through TextBox is UTF-8 safe");
    f.Key(InputKeyCode::A, InputModifiers::Control);
    f.Key(InputKeyCode::C, InputModifiers::Control);
    Expect(f.clipboard.value == aEmoji,
        "Ctrl+C writes Unicode selection");
    f.Key(InputKeyCode::X, InputModifiers::Control);
    Expect(f.runtime.Widgets().GetText(f.box).empty(),
        "Ctrl+X removes the selection");
    f.Key(InputKeyCode::V, InputModifiers::Control);
    Expect(f.runtime.Widgets().GetText(f.box) == aEmoji,
        "Ctrl+V inserts Unicode clipboard text");
    f.Key(InputKeyCode::Z, InputModifiers::Control);
    Expect(f.runtime.Widgets().GetText(f.box).empty(), "Ctrl+Z undoes");
    f.Key(InputKeyCode::Y, InputModifiers::Control);
    Expect(f.runtime.Widgets().GetText(f.box) == aEmoji,
        "Ctrl+Y redoes");

    f.runtime.EnqueueInput(TextCompositionStartEvent{f.window, 0.0});
    f.runtime.EnqueueInput(TextCompositionUpdateEvent{
        f.window, "ni", 2, std::nullopt, 0.0});
    f.Update();
    auto* record = f.runtime.Widgets().Registry().TryGet(f.box);
    Expect(record->textBox->composition.active &&
        f.runtime.Widgets().GetText(f.box) == aEmoji,
        "composition update remains outside committed text");
    const int beforeCommit = f.changes;
    f.runtime.EnqueueInput(TextCompositionCommitEvent{
        f.window, you, 0.0});
    f.runtime.EnqueueInput(TextCompositionEndEvent{f.window, 0.0});
    f.Update();
    Expect(f.runtime.Widgets().GetText(f.box) == aEmoji + you &&
        f.changes == beforeCommit + 1,
        "IME commit is one text change and end does not duplicate it");
    f.runtime.EnqueueInput(TextCompositionStartEvent{f.window, 0.0});
    f.Update(); f.Key(InputKeyCode::Escape);
    Expect(f.ime.cancels == 1 && f.cancels == 0,
        "Escape cancels composition before project cancellation");
    f.Key(InputKeyCode::Escape);
    Expect(f.cancels == 1, "Escape without composition emits cancellation");
    f.Key(InputKeyCode::Enter);
    Expect(f.submits == 1 &&
        f.runtime.Widgets().GetText(f.box).find('\n') == std::string::npos,
        "Enter submits without inserting a newline");

    f.runtime.Widgets().SetReadOnly(f.box, true);
    const std::string readOnlyText = f.runtime.Widgets().GetText(f.box);
    f.Text(U'X'); f.Key(InputKeyCode::V, InputModifiers::Control);
    Expect(f.runtime.Widgets().GetText(f.box) == readOnlyText,
        "read-only TextBox rejects input and paste");
    f.runtime.Widgets().SetReadOnly(f.box, false);
    Expect(f.runtime.Widgets().SetCaret(f.box, 0) &&
        f.runtime.Widgets().SelectAll(f.box) &&
        !f.runtime.Widgets().GetSelection(f.box).Empty(),
        "public caret and selection APIs operate on scalar indices");
    record = f.runtime.Widgets().Registry().TryGet(f.box);
    Expect(record->textBox->hitMap.stops.size() ==
        record->textBox->model.Length() + 1,
        "TextBox hit map comes from actual TextLayout metrics");
    f.runtime.SetVisible(false);
    Expect(!record->textBox->composition.active &&
        !record->textBox->selecting,
        "hidden window clears composition and pointer selection");
    Expect(!f.runtime.QueryFrameActivity().hasActiveAnimation,
        "hidden window stops caret blink activity");
    f.runtime.SetVisible(true); f.Update();
    Expect(!f.runtime.IsFocused(record->rootNode),
        "showing a TextBox window does not invent native focus");
    f.runtime.EnqueueInput(WindowFocusEvent{f.window, true, 0.0});
    f.Update();
    const auto oldRoot = record->rootNode;
    Expect(f.runtime.Widgets().Destroy(f.box), "TextBox destruction queues");
    f.Update();
    Expect(!f.runtime.Scene().TryGet(oldRoot),
        "TextBox destruction releases all owned visual nodes");
    std::cout << "TextBoxFailures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
