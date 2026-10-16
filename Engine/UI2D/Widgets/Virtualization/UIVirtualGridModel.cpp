#include "UI2D/Widgets/Virtualization/UIVirtualGridModel.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Engine::UI2D {
namespace {

float NonNegativeFinite(float value) noexcept {
    return std::isfinite(value) ? std::max(0.0f, value) : 0.0f;
}

std::uint64_t FloorToU64(double value) noexcept {
    if (!(value > 0.0)) return 0;
    const double maximum = static_cast<double>(
        std::numeric_limits<std::uint64_t>::max());
    if (value >= maximum)
        return std::numeric_limits<std::uint64_t>::max();
    return static_cast<std::uint64_t>(std::floor(value));
}

std::uint64_t SaturatingMultiply(
    std::uint64_t left, std::uint64_t right) noexcept {
    if (left == 0 || right == 0) return 0;
    if (left > std::numeric_limits<std::uint64_t>::max() / right)
        return std::numeric_limits<std::uint64_t>::max();
    return left * right;
}

UIVirtualGridRange MakeIndexRange(
    std::uint64_t firstRow,
    std::uint64_t lastRow,
    std::uint32_t columns,
    std::uint64_t itemCount) noexcept {
    if (itemCount == 0 || lastRow < firstRow || columns == 0) return {};
    const std::uint64_t first =
        std::min(SaturatingMultiply(firstRow, columns), itemCount - 1);
    const std::uint64_t rowEnd =
        SaturatingMultiply(lastRow + 1, columns);
    const std::uint64_t last =
        std::min(rowEnd == 0 ? itemCount - 1 : rowEnd - 1, itemCount - 1);
    return {first, last, last - first + 1};
}

} // namespace

UIVirtualGridModel::UIVirtualGridModel(UIVirtualGridMetrics metrics) {
    if (IsValidMetrics(metrics)) m_Metrics = metrics;
}

bool UIVirtualGridModel::IsValidMetrics(
    const UIVirtualGridMetrics& metrics) noexcept {
    return std::isfinite(metrics.cellWidth) && metrics.cellWidth > 0.0f &&
        std::isfinite(metrics.cellHeight) && metrics.cellHeight > 0.0f &&
        std::isfinite(metrics.horizontalSpacing) &&
        metrics.horizontalSpacing >= 0.0f &&
        std::isfinite(metrics.verticalSpacing) &&
        metrics.verticalSpacing >= 0.0f &&
        metrics.minimumColumns > 0 &&
        (metrics.maximumColumns == 0 ||
         metrics.maximumColumns >= metrics.minimumColumns);
}

bool UIVirtualGridModel::SetMetrics(const UIVirtualGridMetrics& metrics) {
    if (!IsValidMetrics(metrics)) return false;
    m_Metrics = metrics;
    return true;
}

UIVirtualGridLayout UIVirtualGridModel::Calculate(
    const UIVirtualGridState& state) const noexcept {
    UIVirtualGridLayout result;
    result.itemCount = state.itemCount;

    const float viewportWidth = NonNegativeFinite(state.viewportWidth);
    const float viewportHeight = NonNegativeFinite(state.viewportHeight);
    const float columnPitch = m_Metrics.cellWidth + m_Metrics.horizontalSpacing;
    const float rowPitch = m_Metrics.cellHeight + m_Metrics.verticalSpacing;
    const double available =
        static_cast<double>(viewportWidth) + m_Metrics.horizontalSpacing;
    std::uint64_t rawColumns = FloorToU64(
        available / columnPitch);
    rawColumns = std::max<std::uint64_t>(
        rawColumns, m_Metrics.minimumColumns);
    if (m_Metrics.maximumColumns != 0)
        rawColumns = std::min<std::uint64_t>(
            rawColumns, m_Metrics.maximumColumns);
    result.columns = static_cast<std::uint32_t>(std::min<std::uint64_t>(
        rawColumns, std::numeric_limits<std::uint32_t>::max()));

    if (state.itemCount == 0) {
        result.contentSize = {viewportWidth, 0.0f};
        return result;
    }

    result.rows =
        state.itemCount / result.columns +
        (state.itemCount % result.columns != 0 ? 1 : 0);
    const double contentHeight =
        static_cast<double>(result.rows) * m_Metrics.cellHeight +
        static_cast<double>(result.rows - 1) * m_Metrics.verticalSpacing;
    result.contentSize = {
        viewportWidth,
        static_cast<float>(std::min<double>(
            contentHeight, std::numeric_limits<float>::max()))};

    if (viewportHeight <= 0.0f) return result;
    const float maximumOffset =
        std::max(0.0f, result.contentSize.y - viewportHeight);
    const float scrollOffset =
        std::clamp(NonNegativeFinite(state.scrollOffsetY), 0.0f, maximumOffset);
    const std::uint64_t firstVisibleRow = std::min<std::uint64_t>(
        FloorToU64(scrollOffset / rowPitch),
        result.rows - 1);
    const float visibleEnd = std::max(
        scrollOffset, scrollOffset + viewportHeight -
        std::numeric_limits<float>::epsilon());
    const std::uint64_t lastVisibleRow = std::min<std::uint64_t>(
        FloorToU64(visibleEnd / rowPitch),
        result.rows - 1);
    result.visible = MakeIndexRange(
        firstVisibleRow, lastVisibleRow, result.columns, state.itemCount);

    const std::uint64_t overscan = m_Metrics.overscanRows;
    const std::uint64_t firstRealizedRow =
        firstVisibleRow > overscan ? firstVisibleRow - overscan : 0;
    const std::uint64_t lastRealizedRow = std::min<std::uint64_t>(
        result.rows - 1, lastVisibleRow + overscan);
    result.realized = MakeIndexRange(
        firstRealizedRow, lastRealizedRow, result.columns, state.itemCount);
    return result;
}

std::optional<Engine::RectF> UIVirtualGridModel::CellRect(
    std::uint64_t index,
    const UIVirtualGridLayout& layout) const noexcept {
    if (index >= layout.itemCount || layout.columns == 0) return std::nullopt;
    const std::uint64_t row = index / layout.columns;
    const std::uint64_t column = index % layout.columns;
    return Engine::RectF{
        static_cast<float>(column) *
            (m_Metrics.cellWidth + m_Metrics.horizontalSpacing),
        static_cast<float>(row) *
            (m_Metrics.cellHeight + m_Metrics.verticalSpacing),
        m_Metrics.cellWidth,
        m_Metrics.cellHeight};
}

} // namespace Engine::UI2D
