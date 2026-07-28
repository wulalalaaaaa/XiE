#pragma once

#include <cstdint>

namespace Engine {

inline constexpr std::uint32_t InvalidHandleIndex = 0;

template <typename Tag>
struct GenerationalHandle {
    std::uint32_t index = InvalidHandleIndex;
    std::uint32_t generation = 0;

    [[nodiscard]] constexpr bool IsValid() const noexcept {
        return index != InvalidHandleIndex && generation != 0;
    }

    friend constexpr bool operator==(GenerationalHandle, GenerationalHandle) = default;
};

} // namespace Engine
