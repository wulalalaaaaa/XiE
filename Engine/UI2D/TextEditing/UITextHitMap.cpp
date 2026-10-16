#include "UI2D/TextEditing/UITextHitMap.h"

#include <algorithm>
#include <cmath>

namespace Engine::UI2D {

std::size_t UITextHitMap::HitTest(float localX) const noexcept {
    if (stops.empty()) return 0;
    if (localX <= stops.front().x) return stops.front().textIndex;
    for (std::size_t i = 1; i < stops.size(); ++i) {
        const float midpoint = (stops[i - 1].x + stops[i].x) * 0.5f;
        if (localX < midpoint) return stops[i - 1].textIndex;
    }
    return stops.back().textIndex;
}

float UITextHitMap::PositionOf(std::size_t index) const noexcept {
    if (stops.empty()) return 0.0f;
    const auto found = std::lower_bound(
        stops.begin(), stops.end(), index,
        [](const UITextCaretStop& stop, std::size_t value) {
            return stop.textIndex < value;
        });
    return found == stops.end() ? stops.back().x : found->x;
}

} // namespace Engine::UI2D
