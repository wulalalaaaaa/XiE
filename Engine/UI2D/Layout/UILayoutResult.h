#pragma once

#include <cstdint>

namespace Engine::UI2D {

enum class UILayoutError {
    None,
    InvalidRoot,
    CycleDetected,
    MaximumDepthExceeded
};

struct UILayoutResult {
    bool layoutChanged = false;
    bool visualInvalidated = false;
    bool hitTestInvalidated = false;
    std::uint32_t measuredNodeCount = 0;
    std::uint32_t arrangedNodeCount = 0;
    UILayoutError error = UILayoutError::None;

    [[nodiscard]] bool Succeeded() const noexcept { return error == UILayoutError::None; }
};

} // namespace Engine::UI2D
