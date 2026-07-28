#include "UI2D/Layout/IUILayoutEngine.h"

#include "UI2D/Core/UIScene.h"

namespace Engine::UI2D {

UILayoutResult PassthroughLayoutEngine::UpdateLayout(UIScene& scene, const UILayoutContext& context) {
    UINodeRecord* root = scene.TryGet(context.layoutRoot.IsValid() ? context.layoutRoot : scene.Root());
    if (!root) {
        UILayoutResult result;
        result.error = UILayoutError::InvalidRoot;
        return result;
    }
    const Engine::RectF next{0, 0, context.logicalWindowSize.x, context.logicalWindowSize.y};
    const bool changed = root->layoutState.arrangedRect != next;
    root->layoutState.desiredSize = context.logicalWindowSize;
    root->layoutState.arrangedRect = next;
    root->layoutState.contentRect = next;
    root->layoutState.sceneRect = next;
    root->layoutState.measureValid = true;
    root->layoutState.arrangeValid = true;
    scene.ClearDirty(UIDirtyFlags::Layout);
    if (changed) scene.MarkDirty(root->handle, UIDirtyFlags::Transform | UIDirtyFlags::Visual | UIDirtyFlags::HitTest);
    UILayoutResult result;
    result.layoutChanged = changed;
    result.visualInvalidated = changed;
    result.hitTestInvalidated = changed;
    result.measuredNodeCount = 1;
    result.arrangedNodeCount = 1;
    return result;
}

} // namespace Engine::UI2D
