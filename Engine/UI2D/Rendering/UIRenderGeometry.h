#pragma once

#include "Foundation/Math/Types2D.h"
#include "UI2D/Core/UINodeRecord.h"

namespace Engine::UI2D {

// Phase 3D clip contract shared by HitTest and rendering. Until transformed
// stencil clips are introduced, clipChildren uses the final Scene-space AABB.
inline Engine::RectF ResolveSceneClipRect(const UINodeRecord& node) noexcept {
    return node.layoutState.sceneRect;
}

} // namespace Engine::UI2D
