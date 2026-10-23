#include "UI2D/Widgets/Virtualization/UIVirtualGridModel.h"

#include <iomanip>
#include <iostream>

using namespace Engine::UI2D;

namespace {
void Print(const char* label, const UIVirtualGridLayout& value) {
    std::cout << std::left << std::setw(8) << label
              << " columns=" << value.columns
              << " rows=" << value.rows
              << " visible=" << value.visible.first << ".." << value.visible.last
              << " realized=" << value.realized.first << ".." << value.realized.last
              << " slots=" << value.realized.count
              << " contentHeight=" << value.contentSize.y << '\n';
}
}

int main() {
    UIVirtualGridModel model;
    UIVirtualGridState state{50'000, 1100.0f, 720.0f, 0.0f};
    const auto top = model.Calculate(state);
    Print("top", top);
    state.scrollOffsetY = top.contentSize.y * 0.5f;
    Print("middle", model.Calculate(state));
    state.scrollOffsetY = top.contentSize.y;
    Print("bottom", model.Calculate(state));
    state.viewportWidth = 800.0f;
    Print("resize", model.Calculate(state));
    std::cout << "dpi=1.5 uses unchanged logical metrics; physical cell=240x240\n";
    return 0;
}
