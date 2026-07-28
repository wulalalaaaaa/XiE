#include "UI2D/Rendering/IUIRenderBuilder.h"

#include "UI2D/Core/UIScene.h"
#include "UI2D/Theme/ResolvedUITheme.h"

namespace Engine::UI2D {

UIRenderResult EmptyUIRenderBuilder::Build(
    const UIScene& scene, const ResolvedUITheme&, Engine::DrawList2D& output, const UIRenderContext&) {
    output.Clear();
    const bool valid = scene.TryGet(scene.Root()) != nullptr;
    UIRenderResult result;
    result.success = valid;
    result.drawListChanged = true;
    result.visitedNodeCount = valid ? static_cast<std::uint32_t>(scene.NodeCount()) : 0;
    return result;
}

} // namespace Engine::UI2D
