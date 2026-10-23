#include "UI2D/Widgets/Virtualization/UIVirtualGridModel.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>

using namespace Engine::UI2D;

namespace {
int failures = 0;
int checks = 0;

void Check(bool value, const char* name) {
    ++checks;
    if (!value) {
        ++failures;
        std::cerr << "FAIL: " << name << '\n';
    }
}

bool Near(float left, float right, float tolerance = 0.01f) {
    return std::abs(left - right) <= tolerance;
}
}

int main() {
    UIVirtualGridModel model;
    Check(UIVirtualGridModel::IsValidMetrics({}), "default metrics valid");
    auto invalid = UIVirtualGridMetrics{};
    invalid.cellWidth = 0.0f;
    Check(!UIVirtualGridModel::IsValidMetrics(invalid), "zero width invalid");
    invalid = {};
    invalid.cellHeight = -1.0f;
    Check(!UIVirtualGridModel::IsValidMetrics(invalid), "negative height invalid");
    invalid = {};
    invalid.horizontalSpacing = -1.0f;
    Check(!UIVirtualGridModel::IsValidMetrics(invalid), "negative h spacing invalid");
    invalid = {};
    invalid.verticalSpacing = -1.0f;
    Check(!UIVirtualGridModel::IsValidMetrics(invalid), "negative v spacing invalid");
    invalid = {};
    invalid.minimumColumns = 0;
    Check(!UIVirtualGridModel::IsValidMetrics(invalid), "zero min columns invalid");
    invalid = {};
    invalid.minimumColumns = 3;
    invalid.maximumColumns = 2;
    Check(!UIVirtualGridModel::IsValidMetrics(invalid), "max below min invalid");

    const auto empty = model.Calculate({});
    Check(empty.itemCount == 0, "empty count");
    Check(empty.rows == 0, "empty rows");
    Check(empty.visible.Empty(), "empty visible");
    Check(empty.realized.Empty(), "empty realized");
    Check(empty.contentSize.y == 0.0f, "empty height");

    UIVirtualGridState state{1, 1100.0f, 720.0f, 0.0f};
    auto one = model.Calculate(state);
    Check(one.columns == 6, "auto columns");
    Check(one.rows == 1, "single row");
    Check(one.visible.count == 1, "single visible");
    Check(one.realized.count == 1, "single realized");
    Check(Near(one.contentSize.y, 160.0f), "single content height");

    state.itemCount = 50'000;
    auto top = model.Calculate(state);
    Check(top.rows == 8'334, "ceil rows");
    Check(top.visible.first == 0, "top first visible");
    Check(top.visible.last == 29, "top last visible");
    Check(top.realized.first == 0, "top overscan start");
    Check(top.realized.last == 41, "top overscan end");
    Check(top.realized.count < 100, "bounded top realization");

    state.scrollOffsetY = top.contentSize.y * 0.5f;
    auto middle = model.Calculate(state);
    Check(middle.visible.first > 20'000, "middle index");
    Check(middle.realized.first < middle.visible.first, "middle leading overscan");
    Check(middle.realized.last > middle.visible.last, "middle trailing overscan");
    Check(middle.realized.count < 100, "bounded middle realization");

    state.scrollOffsetY = top.contentSize.y;
    auto bottom = model.Calculate(state);
    Check(bottom.visible.last == 49'999, "bottom clamps to last");
    Check(bottom.realized.last == 49'999, "bottom realization clamps");
    Check(bottom.realized.count < 100, "bounded bottom realization");

    state.viewportWidth = 800.0f;
    auto resized = model.Calculate(state);
    Check(resized.columns == 4, "resize columns");
    Check(resized.rows == 12'500, "resize rows");
    Check(resized.realized.count < 100, "bounded resized realization");

    UIVirtualGridMetrics capped;
    capped.minimumColumns = 2;
    capped.maximumColumns = 3;
    Check(model.SetMetrics(capped), "set capped metrics");
    state.viewportWidth = 4000.0f;
    auto cappedLayout = model.Calculate(state);
    Check(cappedLayout.columns == 3, "maximum columns clamp");
    state.viewportWidth = 1.0f;
    auto minimumLayout = model.Calculate(state);
    Check(minimumLayout.columns == 2, "minimum columns clamp");

    state.itemCount = 7;
    state.viewportWidth = 500.0f;
    state.viewportHeight = 160.0f;
    state.scrollOffsetY = 0.0f;
    auto partial = model.Calculate(state);
    Check(partial.rows == 3, "partial final row");
    Check(partial.realized.last == 6, "partial final index");
    auto rect0 = model.CellRect(0, partial);
    auto rect4 = model.CellRect(4, partial);
    Check(rect0 && Near(rect0->x, 0.0f) && Near(rect0->y, 0.0f),
        "first rect");
    Check(rect4 && Near(rect4->x, 160.0f + capped.horizontalSpacing) &&
        Near(rect4->y, 160.0f + capped.verticalSpacing), "second row rect");
    Check(!model.CellRect(7, partial), "out of range rect");

    state.viewportHeight = 0.0f;
    auto zeroViewport = model.Calculate(state);
    Check(zeroViewport.visible.Empty(), "zero viewport invisible");
    Check(zeroViewport.realized.Empty(), "zero viewport unrealized");
    state.viewportHeight = 720.0f;
    state.scrollOffsetY = -100.0f;
    Check(model.Calculate(state).visible.first == 0, "negative offset clamped");
    state.scrollOffsetY = std::numeric_limits<float>::infinity();
    Check(model.Calculate(state).visible.first == 0, "nonfinite offset safe");

    model.SetMetrics({});
    state = {100'000, 1100.0f, 720.0f, 0.0f};
    const auto started = std::chrono::steady_clock::now();
    std::uint64_t observed = 0;
    for (int i = 0; i < 1000; ++i) {
        state.scrollOffsetY = static_cast<float>(i) * 137.25f;
        observed += model.Calculate(state).realized.count;
    }
    const auto elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    Check(observed > 0, "performance calculations observed");
    Check(elapsed < 100.0, "1000 O(1) calculations");

    std::cout << "UIVirtualGrid checks=" << checks
              << " failures=" << failures
              << " calculations_ms=" << elapsed << '\n';
    return failures == 0 ? 0 : 1;
}
