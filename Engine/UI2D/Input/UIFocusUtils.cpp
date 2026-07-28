#include "UI2D/Input/UIFocusUtils.h"

#include "UI2D/Core/UIScene.h"

namespace Engine::UI2D {

bool IsInFocusRoot(const UIScene& scene, UINodeHandle node, UINodeHandle focusRoot) {
    if (!scene.TryGet(node) || !scene.TryGet(focusRoot)) return false;
    for (UINodeHandle current = node; current.IsValid();) {
        if (current == focusRoot) return true;
        const UINodeRecord* record = scene.TryGet(current);
        if (!record) return false;
        current = record->parent;
    }
    return false;
}

bool IsFocusable(const UIScene& scene, UINodeHandle node, UINodeHandle focusRoot) {
    const UINodeRecord* record = scene.TryGet(node);
    return record && record->focus.focusable && record->visible && record->enabled &&
        record->layoutState.effectiveVisible && record->layoutState.effectiveEnabled &&
        IsInFocusRoot(scene, node, focusRoot);
}

} // namespace Engine::UI2D
