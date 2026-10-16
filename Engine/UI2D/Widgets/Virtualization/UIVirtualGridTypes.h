#pragma once

#include "Foundation/Math/Types2D.h"

#include <cstddef>
#include <cstdint>

namespace Engine::UI2D {

struct UIVirtualGridMetrics {
    float cellWidth = 160.0f;
    float cellHeight = 160.0f;
    float horizontalSpacing = 8.0f;
    float verticalSpacing = 8.0f;
    std::uint32_t minimumColumns = 1;
    std::uint32_t maximumColumns = 0;
    std::uint32_t overscanRows = 2;

    friend constexpr bool operator==(
        const UIVirtualGridMetrics&, const UIVirtualGridMetrics&) = default;
};

struct UIVirtualGridState {
    std::uint64_t itemCount = 0;
    float viewportWidth = 0.0f;
    float viewportHeight = 0.0f;
    float scrollOffsetY = 0.0f;
};

struct UIVirtualGridRange {
    std::uint64_t first = 0;
    std::uint64_t last = 0;
    std::uint64_t count = 0;

    [[nodiscard]] constexpr bool Empty() const noexcept { return count == 0; }
    [[nodiscard]] constexpr bool Contains(std::uint64_t index) const noexcept {
        return !Empty() && index >= first && index <= last;
    }
    friend constexpr bool operator==(
        UIVirtualGridRange, UIVirtualGridRange) = default;
};

struct UIVirtualGridLayout {
    std::uint64_t itemCount = 0;
    std::uint32_t columns = 1;
    std::uint64_t rows = 0;
    Engine::Vec2F contentSize{};
    UIVirtualGridRange visible{};
    UIVirtualGridRange realized{};
};

struct UIVirtualGridSnapshot : UIVirtualGridLayout {
    std::size_t activeSlots = 0;
    std::size_t pooledSlots = 0;
};

} // namespace Engine::UI2D
