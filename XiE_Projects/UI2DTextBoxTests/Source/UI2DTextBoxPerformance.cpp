#include "Renderer2D/TextLayoutService.h"
#include "UI2D/TextEditing/UITextEditingModel.h"
#include "UI2D/TextEditing/UITextEditingTypes.h"
#include "UI2D/TextEditing/UITextEditingUtils.h"
#include "UI2D/TextEditing/UITextHitMap.h"

#include <chrono>
#include <functional>
#include <iostream>
#include <string>

namespace {

volatile std::size_t sink = 0;

void Measure(
    const char* name, std::size_t operations,
    const std::function<void()>& body) {
    const auto start = std::chrono::steady_clock::now();
    body();
    const double elapsed =
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count();
    std::cout << name << " Operations=" << operations
              << " ElapsedMs=" << elapsed
              << " PerOperationUs="
              << elapsed * 1000.0 /
                    static_cast<double>(operations) << '\n';
}

}

int main() {
    using namespace Engine::UI2D;
    for (const std::size_t length : {16U, 128U, 256U}) {
        UITextEditingModel model(256);
        const std::string text(length, 'x');
        const std::string name =
            "Edit" + std::to_string(length);
        Measure(name.c_str(), 20000, [&] {
            for (int pass = 0; pass < 20000; ++pass) {
                (void)model.SetText(text);
                (void)model.MoveEnd(false);
                if (length < 256) (void)model.InsertText("y");
                (void)model.DeleteBackward();
                sink = model.Length();
            }
        });
    }

    UITextEditingModel navigation(256);
    (void)navigation.SetText(std::string(256, 'x'));
    Measure("CaretMoveAndSelection", 200000, [&] {
        for (int pass = 0; pass < 100000; ++pass) {
            (void)navigation.MoveLeft(true);
            (void)navigation.MoveRight(true);
        }
        sink = navigation.CaretIndex();
    });

    UITextHitMap hitMap;
    for (std::size_t index = 0; index <= 256; ++index)
        hitMap.stops.push_back(
            {index, static_cast<float>(index) * 8.0f});
    Measure("SelectionDragHitTest", 200000, [&] {
        for (int pass = 0; pass < 200000; ++pass)
            sink = hitMap.HitTest(
                static_cast<float>(pass % 2048));
    });

    UITextCompositionState composition;
    Measure("CompositionUpdate", 200000, [&] {
        for (int pass = 0; pass < 200000; ++pass) {
            composition.active = true;
            composition.preeditText =
                (pass & 1) != 0 ? "ni" : "\xE4\xBD\xA0";
            composition.caret = pass & 1;
            sink = composition.preeditText.size();
        }
    });

    Engine::BasicTextLayoutService layouts;
    const auto decoded = DecodeUtf8(std::string(256, 'W'));
    Measure("TextLayoutAndHitMapBuild", 2000, [&] {
        for (int pass = 0; pass < 2000; ++pass) {
            const auto layout = layouts.CreateLayout(
                {1, 1}, *decoded, 4096.0f, 16.0f);
            if (const auto* bitmap = layouts.GetBitmap(layout))
                sink = bitmap->caretStops.size();
        }
    });
    std::cout << "Sink=" << sink << '\n';
    return 0;
}
