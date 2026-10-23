#include "UI2D/TextEditing/UITextEditingModel.h"

#include <iostream>

int main() {
    Engine::UI2D::UITextEditingModel normal;
    Engine::UI2D::UITextEditingModel chinese;
    Engine::UI2D::UITextEditingModel readOnly;
    Engine::UI2D::UITextEditingModel longText;
    chinese.SetText(
        "\xE4\xB8\xAD" "\xE6\x96\x87" "\xE8\xBE\x93"
        "\xE5\x85\xA5" "\xE6\xA1\x86");
    readOnly.SetText("Read only");
    longText.SetText(
        "Long single-line TextBox validates horizontal caret scrolling");
    std::cout
        << "UI2DTextBoxTest=Ready\n"
        << "Variants=Normal,Chinese,ReadOnly,Long,ScrollView,Layered\n"
        << "Chinese=" << chinese.Text() << "\n"
        << "ManualIME=Docs/Testing/UI2DTextBoxIMEManualTest.md\n";
    return 0;
}
