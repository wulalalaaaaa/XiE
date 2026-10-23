#include "Renderer2D/TextLayoutService.h"
#include "UI2D/TextEditing/UITextEditingModel.h"
#include "UI2D/TextEditing/UITextEditingUtils.h"
#include "UI2D/TextEditing/UITextHitMap.h"

#include <iostream>

namespace {
using namespace Engine;
using namespace Engine::UI2D;
int failures = 0;
void Expect(bool value, const char* message) {
    std::cout << (value ? "[PASS] " : "[FAIL] ") << message << '\n';
    if (!value) ++failures;
}
}

int main() {
    const std::string chinese =
        "\xE4\xB8\xAD" "\xE6\x96\x87";
    const std::string japanese =
        "\xE6\x97\xA5" "\xE6\x9C\xAC" "\xE8\xAA\x9E";
    const std::string korean =
        "\xED\x95\x9C" "\xEA\xB8\x80";
    const std::string emoji = "\xF0\x9F\x98\x80";
    const std::string mixed = "A" + chinese.substr(0, 3) + emoji;
    Expect(IsValidUtf8("ASCII"), "ASCII is valid UTF-8");
    Expect(IsValidUtf8(chinese), "Chinese is valid UTF-8");
    Expect(IsValidUtf8(japanese), "Japanese is valid UTF-8");
    Expect(IsValidUtf8(korean), "Korean is valid UTF-8");
    Expect(IsValidUtf8(emoji), "single-scalar emoji is valid UTF-8");
    const std::string invalid("\xE4\xB8", 2);
    Expect(!IsValidUtf8(invalid), "truncated UTF-8 is rejected");
    Expect(Utf8ScalarCount(mixed) == 3,
        "scalar count is independent of UTF-8 byte length");
    Expect(Utf8ByteOffset(mixed, 2).value_or(0) == 4,
        "scalar index converts only to a valid byte boundary");

    UITextEditingModel model(8);
    Expect(model.InsertText(chinese) && model.Length() == 2,
        "Chinese insertion uses scalar indices");
    Expect(model.InsertText(emoji) && model.Length() == 3,
        "emoji insertion preserves UTF-8");
    Expect(model.MoveLeft(false) && model.CaretIndex() == 2,
        "left moves by one scalar");
    Expect(model.DeleteBackward() &&
        model.Text() == chinese.substr(0, 3) + emoji,
        "backspace removes a complete scalar");
    Expect(model.MoveHome(false) && model.DeleteForward() &&
        model.Text() == emoji, "delete removes a complete scalar");
    Expect(model.MoveEnd(false) && model.InsertText("abc"),
        "end and ASCII insert work");
    model.SetSelection({1, 3});
    const std::string sun = "\xE6\x97\xA5";
    Expect(model.ReplaceSelection(sun) &&
        model.Text() == emoji + sun + "c",
        "selection replacement is scalar safe");
    Expect(model.SelectAll() && model.Selection().Start() == 0 &&
        model.Selection().End() == 3, "select all covers scalar range");
    Expect(model.ReplaceSelection("x") && model.Text() == "x",
        "replace all collapses selection");
    Expect(model.Undo() && model.Text() == emoji + sun + "c",
        "undo restores text and selection");
    Expect(model.Redo() && model.Text() == "x",
        "redo restores the edit");
    Expect(!model.InsertText(invalid), "invalid UTF-8 edit is rejected");
    Expect(!model.InsertText("12345678"),
        "max scalar length rejects oversized edits");
    model.SetSelection({0, 0});
    Expect(!model.MoveLeft(false), "left clamps at start");
    Expect(model.MoveEnd(true) && model.Selection().anchor == 0,
        "shift-end preserves anchor");

    UITextEditingModel capped(256);
    for (int i = 0; i < 70; ++i) {
        capped.InsertText("a");
        capped.DeleteBackward();
    }
    Expect(capped.UndoDepth() == 64, "undo history is capped at 64");

    BasicTextLayoutService layouts;
    const auto wide = DecodeUtf8(
        "iW" + chinese.substr(0, 3)).value();
    const auto handle = layouts.CreateLayout(
        {1, 1}, wide, 512.0f, 18.0f);
    const auto* bitmap = layouts.GetBitmap(handle);
    Expect(bitmap && bitmap->caretStops.size() == 4,
        "text layout retains one caret stop per scalar boundary");
    Expect(bitmap && bitmap->caretStops[2] > bitmap->caretStops[1],
        "caret stops are monotonic");
#ifdef _WIN32
    Expect(bitmap &&
        (bitmap->caretStops[1] - bitmap->caretStops[0]) !=
        (bitmap->caretStops[2] - bitmap->caretStops[1]),
        "Windows hit map uses variable-width glyph measurement");
#endif
    UITextHitMap hitMap{{{0, 0.0f}, {1, 4.0f}, {2, 18.0f}}};
    Expect(hitMap.HitTest(3.0f) == 1 &&
        hitMap.HitTest(12.0f) == 2,
        "pointer hit testing uses adjacent caret midpoints");
    Expect(hitMap.PositionOf(1) == 4.0f,
        "caret index maps back to measured x");
    std::cout << "TextEditingFailures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
