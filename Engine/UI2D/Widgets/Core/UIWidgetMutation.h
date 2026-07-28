#pragma once

#include "UI2D/Widgets/Core/UIWidgetTypes.h"

#include <variant>
#include <vector>

namespace Engine::UI2D {

class UIWindowRuntime;
class UIWidgetRegistry;

struct DestroyWidgetMutation { UIWidgetHandle widget{}; };
struct ReparentWidgetMutation { UIWidgetHandle child{}; UIWidgetHandle parent{}; };
struct SetWidgetVisibleMutation { UIWidgetHandle widget{}; bool visible = true; };
struct SetWidgetEnabledMutation { UIWidgetHandle widget{}; bool enabled = true; };
using UIWidgetMutation = std::variant<DestroyWidgetMutation, ReparentWidgetMutation,
    SetWidgetVisibleMutation, SetWidgetEnabledMutation>;

class UIWidgetMutationQueue {
public:
    void Enqueue(UIWidgetMutation mutation) { m_Pending.push_back(std::move(mutation)); }
    UIWidgetUpdateResult Flush(UIWidgetRegistry& registry, UIWindowRuntime& runtime);
    [[nodiscard]] bool Empty() const noexcept { return m_Pending.empty(); }
    [[nodiscard]] std::size_t Size() const noexcept { return m_Pending.size(); }
    void Clear() { m_Pending.clear(); }
private:
    std::vector<UIWidgetMutation> m_Pending;
};

} // namespace Engine::UI2D
