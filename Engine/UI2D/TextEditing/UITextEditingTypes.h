#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>

namespace Engine::UI2D {

// All indices are Unicode scalar indices, never UTF-8 byte offsets.
struct UITextSelection {
    std::size_t anchor = 0;
    std::size_t active = 0;

    [[nodiscard]] bool Empty() const noexcept { return anchor == active; }
    [[nodiscard]] std::size_t Start() const noexcept {
        return std::min(anchor, active);
    }
    [[nodiscard]] std::size_t End() const noexcept {
        return std::max(anchor, active);
    }
};

struct UITextCompositionState {
    bool active = false;
    std::string preeditText;
    std::size_t caret = 0;
    std::optional<UITextSelection> selection;
};

struct UITextEditSnapshot {
    std::string text;
    UITextSelection selection;
};

} // namespace Engine::UI2D
