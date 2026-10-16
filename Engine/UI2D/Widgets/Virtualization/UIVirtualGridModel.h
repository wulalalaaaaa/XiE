#pragma once

#include "UI2D/Widgets/Virtualization/UIVirtualGridTypes.h"

#include <optional>

namespace Engine::UI2D {

class UIVirtualGridModel {
public:
    explicit UIVirtualGridModel(UIVirtualGridMetrics metrics = {});

    [[nodiscard]] static bool IsValidMetrics(
        const UIVirtualGridMetrics& metrics) noexcept;
    bool SetMetrics(const UIVirtualGridMetrics& metrics);
    [[nodiscard]] const UIVirtualGridMetrics& Metrics() const noexcept {
        return m_Metrics;
    }

    [[nodiscard]] UIVirtualGridLayout Calculate(
        const UIVirtualGridState& state) const noexcept;
    [[nodiscard]] std::optional<Engine::RectF> CellRect(
        std::uint64_t index, const UIVirtualGridLayout& layout) const noexcept;

private:
    UIVirtualGridMetrics m_Metrics;
};

} // namespace Engine::UI2D
