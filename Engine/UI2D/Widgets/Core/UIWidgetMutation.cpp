#include "UI2D/Widgets/Core/UIWidgetMutation.h"

#include "UI2D/Runtime/UIWindowRuntime.h"
#include "UI2D/Widgets/Core/UIWidgetRegistry.h"

#include <type_traits>

namespace Engine::UI2D {

UIWidgetUpdateResult UIWidgetMutationQueue::Flush(
    UIWidgetRegistry& registry, UIWindowRuntime& runtime) {
    UIWidgetUpdateResult result;
    if (m_Pending.empty()) return result;
    std::vector<UIWidgetMutation> pending;
    pending.swap(m_Pending);
    for (UIWidgetMutation& item : pending) {
        bool destroyed = false;
        const bool applied = std::visit([&](auto& mutation) -> bool {
            using T = std::decay_t<decltype(mutation)>;
            if constexpr (std::is_same_v<T, DestroyWidgetMutation>) {
                destroyed = registry.Destroy(mutation.widget, runtime);
                return destroyed;
            } else if constexpr (std::is_same_v<T, ReparentWidgetMutation>) {
                return registry.Reparent(mutation.child, mutation.parent, runtime);
            } else if constexpr (std::is_same_v<T, SetWidgetVisibleMutation>) {
                UIWidgetRecord* record = registry.TryGet(mutation.widget);
                if (!record) return false;
                record->visible = mutation.visible;
                return runtime.Scene().SetVisibility(record->rootNode, mutation.visible);
            } else {
                UIWidgetRecord* record = registry.TryGet(mutation.widget);
                if (!record) return false;
                record->enabled = mutation.enabled;
                return runtime.Scene().SetEnabled(record->rootNode, mutation.enabled);
            }
        }, item);
        if (applied) { ++result.appliedMutationCount; result.sceneChanged = true; }
        else ++result.rejectedMutationCount;
        if (destroyed) ++result.destroyedWidgetCount;
    }
    return result;
}

} // namespace Engine::UI2D
