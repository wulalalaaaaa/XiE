#pragma once

#include <cstddef>
#include <vector>

namespace Engine::UI2D {

struct UITextCaretStop {
    std::size_t textIndex = 0;
    float x = 0.0f;
};

struct UITextHitMap {
    std::vector<UITextCaretStop> stops;

    [[nodiscard]] std::size_t HitTest(float localX) const noexcept;
    [[nodiscard]] float PositionOf(std::size_t index) const noexcept;
};

} // namespace Engine::UI2D
