#pragma once

#include <cstdint>

namespace Engine::UI2D {

enum class UIDirtyFlags : std::uint32_t {
    None = 0,
    Layout = 1u << 0u,
    Transform = 1u << 1u,
    Visual = 1u << 2u,
    Children = 1u << 3u,
    HitTest = 1u << 4u,
    All = (1u << 5u) - 1u
};

constexpr UIDirtyFlags operator|(UIDirtyFlags lhs, UIDirtyFlags rhs) {
    return static_cast<UIDirtyFlags>(static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
}
constexpr UIDirtyFlags operator&(UIDirtyFlags lhs, UIDirtyFlags rhs) {
    return static_cast<UIDirtyFlags>(static_cast<std::uint32_t>(lhs) & static_cast<std::uint32_t>(rhs));
}
constexpr UIDirtyFlags operator~(UIDirtyFlags value) {
    return static_cast<UIDirtyFlags>(~static_cast<std::uint32_t>(value));
}
inline UIDirtyFlags& operator|=(UIDirtyFlags& lhs, UIDirtyFlags rhs) { return lhs = lhs | rhs; }
inline UIDirtyFlags& operator&=(UIDirtyFlags& lhs, UIDirtyFlags rhs) { return lhs = lhs & rhs; }
constexpr bool HasAny(UIDirtyFlags value, UIDirtyFlags flags) { return (value & flags) != UIDirtyFlags::None; }

} // namespace Engine::UI2D
