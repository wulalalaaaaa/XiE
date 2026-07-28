#pragma once

#include "UI2D/Core/UINodeHandle.h"

#include <cstdint>

namespace Engine::UI2D {

// Shared visual traversal contract. Entries are in back-to-front painter order:
// a parent precedes its children, and siblings are ordered by zOrder then stable
// insertion order.
struct UITraversalEntry {
    UINodeHandle node{};
    UINodeHandle parent{};
    std::uint64_t painterOrder = 0;
    std::uint32_t depth = 0;
};

} // namespace Engine::UI2D
